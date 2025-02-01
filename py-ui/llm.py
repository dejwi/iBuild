import gc
import re
from typing import Any, Callable

from llama_cpp import Llama
from utils import llm_system_prompt


class Mixin:
    update_progress: Callable[[str, int], Any]
    llm_model_local_path: str

    def run_llm(self, prompt, ai_description):
        self.update_progress("Init llm", 45)
        llm = Llama(
            model_path=self.llm_model_local_path,
            n_ctx=8000,
            n_threads=8,
            n_gpu_layers=8,
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

        del llm
        gc.collect()
        return cleaned_text
