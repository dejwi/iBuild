import gc
import re
import importlib
from typing import Any, Callable

from prompts import llm_system_prompt
from utils import get_path_exc, device


class Mixin:
    update_progress: Callable[[str, int], Any]
    llm_model_local_path: str

    def run_llm(self, prompt, ai_description, cpu_threads, gpu_layers):
        self.update_progress("Initializng llm", 45)

        if device == "cuda":
                llama_lib = importlib.import_module("llama_cpp_cuda")
        else:
                llama_lib = importlib.import_module("llama_cpp")

        llm = llama_lib.Llama(
            model_path=self.llm_model_local_path,
            n_ctx=8100,
            n_threads=cpu_threads,
            n_gpu_layers=gpu_layers,
            verbose=False,
        )
        self.update_progress("Generating final dataset", 60)

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

        with open(get_path_exc("./generated_samples/llm_full.txt"), "w") as f:
            f.write(text)
        with open(get_path_exc("./generated_samples/llm_clean.txt"), "w") as f:
            f.write(cleaned_text)

        del llm
        gc.collect()
        return cleaned_text
