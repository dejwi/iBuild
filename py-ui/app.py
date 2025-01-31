import gc
import os
import re
import threading
import tkinter as tk
from tkinter import messagebox, ttk
from typing import Any, Callable, Optional

import numpy as np
import torch
from huggingface_hub import hf_hub_download, snapshot_download
from janus.models import MultiModalityCausalLM, VLChatProcessor
from janus.models.processing_vlm import VLChatProcessorOutput
from janus.utils.io import load_pil_images
from llama_cpp import Llama
from PIL import Image, ImageTk
from tqdm.auto import tqdm
from transformers import AutoConfig, AutoModelForCausalLM, AutoTokenizer

if torch.cuda.is_available():
    device = "cuda"
    dtype = torch.bfloat16
elif torch.backends.mps.is_available():
    device = "mps"
    dtype = torch.float16
else:
    device = "cpu"
    dtype = torch.float16


def clear_torch_cache():
    torch.cuda.empty_cache()
    torch.mps.empty_cache()


class AIApplication:
    def __init__(self, root):
        self.root = root
        root.title("AI Dataset Creator")

        self.create_widgets()
        # self.setup_threading()

        # Model paths
        # self.janus_model_path = "models/deepseek-ai/Janus-Pro-7B"
        self.janus_model_path = "deepseek-ai/Janus-Pro-1B"
        self.llm_model_path = {
            "repo_id": "lmstudio-community/DeepSeek-R1-Distill-Qwen-7B-GGUF",
            "filename": "DeepSeek-R1-Distill-Qwen-7B-Q4_K_M.gguf",
        }
        self.janus_model_local_path = os.path.join("./models", self.janus_model_path)
        self.llm_model_local_path = os.path.join(
            "./models", self.llm_model_path["filename"]
        )

        # Model placeholders
        self.janus_model: MultiModalityCausalLM | None = None
        self.janus_processor: Any = None
        self.llm_model = None
        self.reasoning_tokenizer = None

        self.patch_tqm()

    def create_widgets(self):
        self.prompt_label = tk.Label(self.root, text="Enter Prompt:")
        self.prompt_label.pack(pady=5)

        self.prompt_entry = tk.Entry(self.root, width=80)
        self.prompt_entry.pack(pady=5)

        self.progress_label = tk.Label(self.root, text="Ready")
        self.progress_label.pack(pady=5)

        self.progress_bar = ttk.Progressbar(
            self.root, orient=tk.HORIZONTAL, length=400, mode="determinate"
        )
        self.progress_bar.pack(pady=5)

        self.generate_btn = tk.Button(
            self.root, text="Generate", command=self.start_generation
        )
        self.generate_btn.pack(pady=10)

        self.image_label = tk.Label(self.root)
        self.image_label.pack(pady=5)

        self.output_text = tk.Text(self.root, height=10, width=80)
        self.output_text.pack(pady=5)

    def start_generation(self):
        self.generate_btn.config(state=tk.DISABLED)
        thread = threading.Thread(target=self.generate_process)
        thread.start()

    def update_progress(self, message=None, value=None):
        self.root.after(0, lambda: self._update_progress(message, value))

    def _update_progress(self, message, value):
        if message is not None:
            self.progress_label.config(text=message)
        if value is not None:
            self.progress_bar["value"] = value
        self.root.update_idletasks()

    def patch_tqm(self):
        update_progress = self.update_progress
        original_update = tqdm.update

        def patched_update(self: tqdm, n=1):
            if self.n is not None and self.total is not None:
                # Format the information
                downloaded = self.n / 1024 / 1024
                total_size = self.total / 1024 / 1024
                speed = (
                    self.format_dict["rate"] / 1024 / 1024
                    if self.format_dict["rate"]
                    else 0.001
                )
                # time_left = format_time((total_size - downloaded) / speed)
                # progress_bar.progress(
                #     self.n / self.total,
                #     f"Downloaded: {int(downloaded)}/{int(total_size)} MB Speed: {speed:.2f} MB/s Remaining: {time_left}",
                # )
                update_progress(
                    f"{self.desc}: {int(downloaded)}/{int(total_size)} MB Speed: {speed:.2f} MB/s",
                    (self.n / self.total) * 100,
                )

            return original_update(self, n)

        tqdm.update = patched_update  # type: ignore

    def generate_process(self):
        try:
            user_prompt = self.prompt_entry.get()

            # Step 1: Generate image with Janus
            # image, description = self.run_janus_steps(user_prompt)
            # print(description)

            # Step 2: Generate final output with Qwen
            # final_output = self.run_qwen_steps(user_prompt, description)
            ret = self.run_llm("Tell me about roses", "pretty red roses")
            print(ret)

            self.update_progress(value=100)

        except Exception as e:
            # self.queue.put({'type': 'error', 'text': str(e)})
            print(e)
        finally:
            self.running = False

    def download_model(
        self, repo_id: str, local_dir: str, filename: Optional[str] = None
    ):
        try:
            if filename:
                return hf_hub_download(
                    repo_id=repo_id,
                    filename=filename,
                    local_dir=local_dir,
                    # progress=self.download_progress
                )
            else:
                path = snapshot_download(
                    repo_id=repo_id,
                    local_dir=local_dir,
                    # progress=self.download_progress
                )
                return path
        except Exception as e:
            messagebox.showerror("Download Error", str(e))
            return None

    # def run_qwen_steps(self, prompt, description):
    #     # Check and download model if needed
    #     if not os.path.exists(self.qwen_model_path):
    #         self.download_model(
    #             repo_id="Qwen2.5-7B-Instruct-1M-GGUF",
    #             filename="Qwen2.5-7B-Instruct-1M-Q4_K_M.gguf",
    #             local_dir=os.path.dirname(self.qwen_model_path),
    #             download_type="file"
    #         )
    #
    #     # Load model
    #     qwen_model = Llama(
    #         model_path=self.qwen_model_path,
    #         n_ctx=2048,
    #         n_threads=4
    #     )
    #
    #     # Generate output
    #     self.update_progress(80)
    #     final_output = self.generate_final_output(prompt, description, qwen_model)
    #
    #     # Unload model and clean up
    #     del qwen_model
    #     gc.collect()
    #
    #     return final_output
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

    def run_llm(self, prompt, ai_description):
        self.download_model(
            repo_id=self.llm_model_path["repo_id"],
            filename=self.llm_model_path["filename"],
            local_dir=self.llm_model_local_path,
        )
        self.update_progress("Init llm", 45)
        llm = Llama(
            model_path=self.llm_model_local_path,
            n_ctx=5000,
            n_threads=8,
            n_gpu_layers=8,
            verbose=False,
        )
        combined_prompt = f"Prompt: {prompt}\nDescription: {ai_description}\n"
        self.update_progress("Generate llm", 50)
        output = llm(combined_prompt, max_tokens=5000)
        text = output["choices"][0]["text"]  # type: ignore
        cleaned_text = re.sub(r".*?</think>\s*", "", text, flags=re.DOTALL).strip()

        del llm
        gc.collect()
        return cleaned_text

    def run_janus_steps(self, prompt):
        # Check and download model if neeed
        # if not os.path.exists(self.janus_model_local_path):
        #     self.download_model(
        #         repo_id=self.janus_model_path,
        #         local_dir=self.janus_model_local_path
        #         # download_type="snapshot"
        #     )

        # self.download_model(
        #     repo_id=self.janus_model_path,
        #     local_dir=self.janus_model_local_path,
        #     # download_type="snapshot"
        # )

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

    def generate_final_output(self, prompt, description, model):
        combined_prompt = f"""Original Prompt: {prompt}
        Image Description: {description}
        Please provide a detailed analysis and reasoning based on the above information."""

        response = model(combined_prompt, max_tokens=500, stop=["</s>"], echo=False)

        output = response["choices"][0]["text"].strip()
        # self.queue.put({'type': 'output', 'text': output})
        return output


if __name__ == "__main__":
    root = tk.Tk()
    app = AIApplication(root)
    root.mainloop()
