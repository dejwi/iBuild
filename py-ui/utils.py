import os
import sys

import torch

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
    if torch.cuda.is_available():
        torch.cuda.empty_cache()
    if torch.backends.mps.is_available():
        torch.mps.empty_cache()


def get_path_exc(base):
    if getattr(sys, "frozen", False):
        # Pyinstaller build
        if sys.platform == "darwin":
            end = sys.executable.rfind("/iBuild.app")
            dir = sys.executable[:end]
            return os.path.join(dir, base)
        return os.path.join(os.path.dirname(sys.executable), base)
    else:
        return os.path.join(os.path.dirname(__file__), "../", base)
