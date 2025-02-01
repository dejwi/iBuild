import gc
import os
from typing import Any, Callable

import numpy as np
import torch
from janus.models import MultiModalityCausalLM, VLChatProcessor
from janus.models.modeling_vlm import MultiModalityCausalLM
from PIL import Image
from transformers import AutoConfig, AutoModelForCausalLM
from utils import clear_torch_cache, device, dtype


class Mixin:
    janus_model_local_path: str
    janus_model: MultiModalityCausalLM | None
    janus_processor: Any
    update_progress: Callable[[str, int], Any]

    def load_janus_model(self):
        config = AutoConfig.from_pretrained(self.janus_model_local_path)
        language_config = config.language_config
        language_config._attn_implementation = "eager"

        model = AutoModelForCausalLM.from_pretrained(
            self.janus_model_local_path,
            language_config=language_config,
            trust_remote_code=True,
        )
        model = model.to(dtype).to(device).eval()
        processor = VLChatProcessor.from_pretrained(self.janus_model_local_path)

        self.janus_model = model
        self.janus_processor = processor

    def run_janus_steps(self, prompt):
        self.update_progress("Initializing multimodal AI", 0)
        # Load model
        if not self.janus_model:
            self.load_janus_model()

        assert self.janus_model is not None

        # Generate image
        self.update_progress("Generating image", 15)
        image = self.generate_image(prompt)

        # Generate description
        self.update_progress("Generating description for the image", 40)
        description = self.multimodal_understanding(
            image, "Describe the provided image"
        )
        # description = self.generate_image_description(image, janus_model, tokenizer)

        # Unload model and clean up
        self.janus_model.cpu()
        del self.janus_processor
        del self.janus_model
        self.janus_processor = None
        self.janus_model = None
        gc.collect()
        clear_torch_cache()

        return image, description

    @torch.inference_mode()
    # Multimodal Understanding function
    def multimodal_understanding(self, image, question):
        assert self.janus_model is not None
        assert self.janus_processor is not None
        # Clear cache before generating
        clear_torch_cache()

        tokenizer = self.janus_processor.tokenizer
        conversation = [
            {
                "role": "<|User|>",
                "content": f"<image_placeholder>\n{question}",
                "images": [image],
            },
            {"role": "<|Assistant|>", "content": ""},
        ]

        pil_images = [Image.fromarray(image)]
        prepare_inputs = self.janus_processor(
            conversations=conversation, images=pil_images, force_batchify=True
        ).to(
            device,
            dtype=dtype,
        )

        inputs_embeds = self.janus_model.prepare_inputs_embeds(**prepare_inputs)

        outputs = self.janus_model.language_model.generate(
            inputs_embeds=inputs_embeds,
            attention_mask=prepare_inputs.attention_mask,
            pad_token_id=tokenizer.eos_token_id,
            bos_token_id=tokenizer.bos_token_id,
            eos_token_id=tokenizer.eos_token_id,
            max_new_tokens=512,
            # do_sample=False if temperature == 0 else True,
            do_sample=False,
            use_cache=True,
            # temperature=temperature,
            # top_p=top_p,
        )

        answer = tokenizer.decode(outputs[0].cpu().tolist(), skip_special_tokens=True)
        return answer

    def generate_janus(
        self,
        input_ids,
        width,
        height,
        temperature: float = 1,
        parallel_size: int = 1,
        cfg_weight: float = 7.5,
        image_token_num_per_image: int = 576,
        patch_size: int = 16,
    ):
        assert self.janus_model is not None
        assert self.janus_processor is not None
        # Clear cache before generating
        clear_torch_cache()

        tokens = torch.zeros((parallel_size * 2, len(input_ids)), dtype=torch.int).to(
            device
        )
        for i in range(parallel_size * 2):
            tokens[i, :] = input_ids
            if i % 2 != 0:
                tokens[i, 1:-1] = self.janus_processor.pad_id
        inputs_embeds = self.janus_model.language_model.get_input_embeddings()(tokens)
        generated_tokens = torch.zeros(
            (parallel_size, image_token_num_per_image), dtype=torch.int
        ).to(device)

        pkv = None
        for i in range(image_token_num_per_image):
            with torch.no_grad():
                outputs = self.janus_model.language_model.model(
                    inputs_embeds=inputs_embeds, use_cache=True, past_key_values=pkv
                )
                pkv = outputs.past_key_values
                hidden_states = outputs.last_hidden_state
                logits = self.janus_model.gen_head(hidden_states[:, -1, :])
                logit_cond = logits[0::2, :]
                logit_uncond = logits[1::2, :]
                logits = logit_uncond + cfg_weight * (logit_cond - logit_uncond)
                probs = torch.softmax(logits / temperature, dim=-1)
                next_token = torch.multinomial(probs, num_samples=1)
                generated_tokens[:, i] = next_token.squeeze(dim=-1)
                next_token = torch.cat(
                    [next_token.unsqueeze(dim=1), next_token.unsqueeze(dim=1)], dim=1
                ).view(-1)

                img_embeds = self.janus_model.prepare_gen_img_embeds(next_token)  # type: ignore
                inputs_embeds = img_embeds.unsqueeze(dim=1)

        patches = self.janus_model.gen_vision_model.decode_code(
            generated_tokens.to(dtype=torch.int),
            shape=[parallel_size, 8, width // patch_size, height // patch_size],
        )  # type: ignore

        return generated_tokens.to(dtype=torch.int), patches

    def unpack_image(self, dec, width, height, parallel_size=5):
        dec = dec.to(torch.float32).cpu().numpy().transpose(0, 2, 3, 1)
        dec = np.clip((dec + 1) / 2 * 255, 0, 255)

        visual_img = np.zeros((parallel_size, width, height, 3), dtype=np.uint8)
        visual_img[:, :, :] = dec

        return visual_img

    @torch.inference_mode()
    # @spaces.GPU(duration=120)  # Specify a duration to avoid timeout
    def generate_image(self, prompt, seed=None, guidance=5, t2i_temperature=1.0):
        assert self.janus_processor is not None

        # Clear cache and avoid tracking gradients
        clear_torch_cache()

        # Set the seed for reproducible results
        if seed is not None:
            torch.manual_seed(seed)
            torch.cuda.manual_seed(seed)
            np.random.seed(seed)
        width = 384
        height = 384
        parallel_size = 1

        with torch.no_grad():
            messages = [
                {"role": "<|User|>", "content": prompt},
                {"role": "<|Assistant|>", "content": ""},
            ]
            text = self.janus_processor.apply_sft_template_for_multi_turn_prompts(
                conversations=messages,
                sft_format=self.janus_processor.sft_format,
                system_prompt="",
            )
            text = text + self.janus_processor.image_start_tag
            tokenizer = self.janus_processor.tokenizer
            input_ids = torch.LongTensor(tokenizer.encode(text))
            output, patches = self.generate_janus(
                input_ids,
                width // 16 * 16,
                height // 16 * 16,
                cfg_weight=guidance,
                parallel_size=parallel_size,
                temperature=t2i_temperature,
            )
            images = self.unpack_image(
                patches,
                width // 16 * 16,
                height // 16 * 16,
                parallel_size=parallel_size,
            )
            os.makedirs("./generated_samples", exist_ok=True)
            save_path = os.path.join("./generated_samples", "img_{}.jpg".format(0))
            Image.fromarray(images[0]).save(save_path)
            return images[0]
            # return [Image.fromarray(images[i]).resize((768, 768), Image.LANCZOS) for i in range(parallel_size)]
