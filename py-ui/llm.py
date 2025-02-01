import gc
import re
from typing import Any, Callable

from llama_cpp import Llama
from utils import llm_system_prompt


class Mixin:
    update_progress: Callable[[str, int], Any]
    llm_model_local_path: str

    def run_llm(self, prompt, ai_description, cpu_threads, gpu_layers):
        self.update_progress("Init llm", 45)
        llm = Llama(
            model_path=self.llm_model_local_path,
            n_ctx=8100,
            n_threads=cpu_threads,
            n_gpu_layers=gpu_layers,
            verbose=False,
        )
        # combined_prompt = f"Prompt: {prompt}\nDescription: {ai_description}\n"
        self.update_progress("Generate final dataset", 60)
        # output = llm(combined_prompt, max_tokens=5000)
        output = llm.create_chat_completion(
            messages=[
                {
                    "role": "system",
                    "content": llm_system_prompt,
                },
                {
                    "role": "user",
                    "content": f"User Prompt: `minecraft {prompt}`\n Ai description: `{ai_description}`",
                },
            ],
            temperature=0.7,
        )
        text: str = output["choices"][0]["message"]["content"]  # type: ignore
        cleaned_text = re.sub(r".*?</think>\s*", "", text, flags=re.DOTALL).strip()

        with open("./generated_samples/llm_full.txt", "w") as f:
            f.write(text)
        with open("./generated_samples/llm_clean.txt", "w") as f:
            f.write(cleaned_text)

        del llm
        gc.collect()
        return cleaned_text
