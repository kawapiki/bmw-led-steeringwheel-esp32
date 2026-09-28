TGX v1.1.4, https://github.com/vindar/tgx
Commit d43f88906ab87f877ea249af2232896b097f15a7
src/ and LGPL-2.1-or-later LICENSE vendored for reproducible ESP-IDF builds.
Firmware source and pinned library source permit rebuilding/relinking with modified TGX.

Local patch: Renderer3D.inl annotates intentional clipping switch fallthrough for GCC15 -Werror. No behavior change.
