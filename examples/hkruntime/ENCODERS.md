# Encoder decisions (2026-10-23)

Gemma 4's vision and audio encoders are assimilated by hodgemix (`assimtowers.go`, D2/D7). Their
linears are ternarised with limbs, and everything else is kept as stored. The bridge
`bnzmix.embed_bridge` [D, 1536] is vocab.go `residualMap` from Gemma's token embedding onto
Hodge's, fitted over the token strings both tokenizers share. hkruntime runs both encoders on
WebGPU (`model/encoder.go`, `model/audio.go`, `kernels/enc*.wgsl`).

The checkpoint fixes the tensors and their shapes. The order of operations between them is
recorded here. Items marked * are inferred, not read from a file; each can be overridden.

| # | Decision |
|---|---|
| E1* | Image: aspect kept, a grid of 16 px patches whose sides are multiples of 3, at most 2520 patches (280 tokens × 3 × 3). Pixels scaled to [-1, 1]; no mean/std (`standardize: false`). Patch values in (row, column, channel) order. |
| E2 | `patch_embedder.input_proj`, then `x += position_embedding_table[0][col] + position_embedding_table[1][row]`. |
| E3* | Per layer: `h = x + post_attention_layernorm(attn(input_layernorm(x)))`, `x = h + post_feedforward_layernorm(mlp(pre_feedforward_layernorm(h)))`. RMS norms times w (not 1 + w), eps 1e-6. |
| E4* | Bidirectional attention over all patches. Per-head q/k RMS norm (`q_norm`, `k_norm`). 2-D RoPE, θ 100: the first half of each head turns with the column, the second with the row, rotate-half within each half. Scale 1. |
| E5 | MLP `down(gelu_tanh(gate(x)) · up(x))`. Every clipped linear clamps its input to `[input_min, input_max]` and its output to `[output_min, output_max]`. |
| E6* | 3 × 3 mean pool, unit RMS norm, `embed_vision` (768 → 1536), then the bridge. |
| E7* | Audio front end (`tower.wasm` mel): 16 kHz mono, HTK pre-emphasis 0.97, periodic Hann window of 512, hop 160, 1024-point FFT magnitude, 128 HTK mel bands 125–7600 Hz, `log(max(x, 1e-5))`. At most 30 s. |
| E8* | Two 3 × 3 stride-2 convolutions with zero padding 1 (128, then 32 channels). Each has an RMS norm over its channels times its weight, then ReLU. Then [time][frequency][channel] flattens into `input_proj_linear`: one token per 40 ms. |
| E9* | Per block: `x += ½ ffw1(x)`; `x += attn(x)`; `x += lconv(x)`; `x += ½ ffw2(x)`; `x = norm_out(x)`. ffw = `post_layer_norm(l2(silu(l1(pre_layer_norm(x)))))`. attn = `norm_post_attn(post(local(norm_pre_attn(x))))`: frame t sees t−12..t; the score is `q·k + q·rel[12−d]`, where rel is `relative_k_proj` of a sin/cos distance signal; q is scaled by `hd^-½ / softplus(0) · softplus(per_dim_scale)`; capped at `50·tanh(s/50)`. lconv = `linear_end(silu(conv_norm(causal depthwise conv k5(glu(linear_start(pre_layer_norm(x)))))))`. |
| E10* | `output_proj` (+ bias), unit RMS norm, `embed_audio` (1536 → 1536), then the bridge. |

Runtime: activations are per-token int8 (absmax), like the text path. Each encoder block is its
own GPU submission, because one command buffer for a large image ran past the GPU watchdog.
Encoder tokens use reserved ids past the vocabulary (images from V, audio from V + 512). Their
vectors stay on the GPU and are copied into the residual stream in place of an embedding row.
