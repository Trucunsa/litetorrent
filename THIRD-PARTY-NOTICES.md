# Third-party notices

The released `litetorrent.exe` statically links the components below. Their
licenses apply to those parts of the binary. This repository itself contains
only LiteTorrent's own source; the build script downloads libtorrent and
expects Boost and OpenSSL to be provided.

| Component | Version | License | Text |
|---|---|---|---|
| [libtorrent](https://github.com/arvidn/libtorrent) (Arvid Norberg and contributors) | 2.0.11 | BSD 3-Clause | [licenses/libtorrent-LICENSE.txt](licenses/libtorrent-LICENSE.txt) |
| [OpenSSL](https://www.openssl.org/) | 3.6.5 | Apache License 2.0 | [licenses/OpenSSL-LICENSE.txt](licenses/OpenSSL-LICENSE.txt) |
| [Boost](https://www.boost.org/) (headers only) | 1.92.0 | Boost Software License 1.0 | [licenses/Boost-LICENSE_1_0.txt](licenses/Boost-LICENSE_1_0.txt) |
| GCC runtime (libstdc++, libgcc) | 13 | GPL 3 with the GCC Runtime Library Exception | https://www.gnu.org/licenses/gcc-exception-3.1.html |
| MinGW-w64 runtime and winpthreads | Debian package | Zope Public License 2.1 / MIT / BSD-style | https://www.mingw-w64.org/ |

libtorrent additionally bundles small third-party pieces of its own
(for example an Ed25519 implementation and the `try_signal` helper); see the
`LICENSE` and `COPYING` files in its source tree for details.

The OpenSSL and Boost binaries/headers used for the release came from the
MSYS2 packages `mingw-w64-x86_64-openssl` 3.6.5-1 and
`mingw-w64-x86_64-boost` 1.92.0-4.
