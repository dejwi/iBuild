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
    torch.cuda.empty_cache()
    torch.mps.empty_cache()
