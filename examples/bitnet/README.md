# bitnet

BitNet b1.58 inference as a Go++ program on the wasigocvm machine, with the
matrix work in WGSL on the browser's WebGPU. It runs Hugging Face's packed
BitNet checkpoints (`BitNetForCausalLM`, `quantization_config` bitnet /
autobitlinear / offline), such as
[microsoft/bitnet-b1.58-2B-4T](https://huggingface.co/microsoft/bitnet-b1.58-2B-4T).
This is the model [microsoft/BitNet](https://github.com/microsoft/BitNet)
serves; its own GPU path converts the bf16 checkpoint to a torch file for a
CUDA kernel, while this reads the published packed safetensors as they are.

| Piece | Where |
| --- | --- |
| Program (flags, chat, sampling) | `main.go` |
| Model: config, upload, the recorded step | `model/model.go` |
| Kernels | `kernels/*.wgsl` |
| Page | `web/index.html` (runs in `web/champ-worker.js`) |
| WebGPU from Go++ | `stdlib/webgpu` over `WASMWebGPU/` |
| Weights and tokenizer | `stdlib/safetensors`, `stdlib/tokenizers` |

## Run it

```
compile.bat examples\bitnet\main.go -o examples\bitnet\web\bitnet.wasm
node WASMJsLoader\serve.mjs --mount ex=examples --mount models=<folder holding the checkpoint>
```

Open `http://127.0.0.1:8792/ex/bitnet/web/index.html`. The page is Encryptid
EdgeAI. The assistant is Champ, named for the lake monster. It loads the checkpoint on open (default
`/models/champ/`) and chats over the guest's `-ui`
protocol. The browser needs WebGPU and JSPI (`WebAssembly.Suspending`); the
page needs cross-origin isolation, which `serve.mjs` sends. Query parameters:
`model=<url>`, `prompt=<text>` (sent as the first message once the model is
ready), and `arg=<flag>` (repeatable) for the flags in `main.go` (`-n`,
`-ctx`, `-temp`, `-top-p`, `-seed`, `-system`, `-debug`). The page is a
PWA (`manifest.webmanifest`). `?model=` still selects the folder. The
product surface does not show a checkpoint control.

The checkpoint folder needs `config.json`, `tokenizer.json`,
`model.safetensors`, and optionally `generation_config.json` (sampling
defaults, end tokens) and `tokenizer_config.json`. Chat uses the format that
model's own tokens describe: the 2B4T `Role: content<|eot_id|>` template,
Llama 3 headers, Gemma `<start_of_turn>`, Qwen `<|im_start|>`, or gpt-oss
harmony. A checkpoint with none of those markers still chats, as plain text.

`model.bnz` is that same file packed with the BNZ2 codec:

```
node web/pack.mjs c model.safetensors model.bnz
```

The page HEADs `model.bnz` and loads it when the file is present. With no
`model.bnz` it loads `model.safetensors`. Pieces of the packed file are
gzipped again on the wire (`/api/bnz/bytes`, or a WebRTC room). The guest
still reads safetensors bytes. `web/host.html` is the operator share tab
for room `champ` (`?room=` and `?bnz=` override it). It is not linked from
the Champ page.

## How a step runs

Per token, one `gocvm.Call("webgpu", "Run" ...)`: write the step uniform
(position, embedding row), embed, then for every layer

```
norm+quant(x, input_layernorm)          -> int8 x, scale
q, k, v = BitLinear                      (bitlinear.wgsl)
rope(q, k); k, v -> cache                (rope.wgsl)
attention over 0..pos                    (attention.wgsl, grouped-query)
norm+quant(attn, attn_sub_norm); x += o_proj
norm+quant(x, post_attention_layernorm)
gate, up = BitLinear
norm+quant(relu(gate)^2 * up, ffn_sub_norm); x += down_proj
```

then the final norm and the tied LM head (bf16 embedding), and the logits
are read back for sampling. The layer part is recorded once at load
(`Model.record`); a prompt runs up to 16 tokens per call.

BitLinear reads the checkpoint's packed weights unchanged: byte k of packed
row r holds the 2-bit codes (weight + 1) of rows r, r + N/4, r + 2N/4,
r + 3N/4 at input k, so one u32 is 4 inputs of 4 rows, `(w >> 2i) &
0x03030303` is row i's codes as 4 bytes, and with int8 activations `a`

```
sum a * (code - 1) = dot4I8Packed(a, codes) - dot4I8Packed(a, 0x01010101)
```

(`dot4_soft.wgsl` when the device lacks `packed_4x8_integer_dot_product`).
Activations are quantized per token by absmax to int8 and the result scaled
by the activation and weight scales, as `AutoBitLinear` does.

## Checked

Against a float32 numpy reference of the same checkpoint (Hugging Face
`tokenizers` for the ids): identical prompt token ids, and the same top
next-token logits within about 0.1 (int8 rounding over 30 layers), e.g.
`The capital of France is` -> ` Paris` 18.58 (reference 18.52). On an AMD
Vega (gcn-5) in Chrome: weights upload in under 10 s, about 17-19 tokens/s
decoding at short context.

## Limits

- Fixed per-token GEMV path: a prompt is processed token by token (16 per
  call), not as one matrix multiply.
- Logits (vocab x f32) are read back every token and sampled in wasm.
- The KV cache is f32 and must fit one binding: `-ctx` is capped by the
  device's `maxStorageBufferBindingSize`.
- Chat formatting is the model's "Role: content<|eot_id|>" template only;
  another template is refused rather than guessed (use `prompt=`).
