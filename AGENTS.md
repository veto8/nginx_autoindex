# AGENTS.md

## Project

Sortable file browser CGI for nginx.

## Tech Stack

- **Language**: C (compiled CGI binary)
- **Web server**: nginx + fcgiwrap
- **Serves**: /mnt/it/files/

## Build & Deploy

```bash
# Compile
gcc -O2 -o files.cgi files.cgi.c

# Deploy to web server
scp files.cgi root@192.168.81.103:/usr/lib/cgi-bin/files.cgi
```

On web server:
```bash
chmod +x /usr/lib/cgi-bin/files.cgi
systemctl restart fcgiwrap
```

## Code Conventions

- Single-file C program, no external dependencies beyond libc
- Base path hardcoded as `#define BASE "/mnt/it/files/"`
- All HTML output via printf (no templates)
- URL encoding/decoding handled manually

## Key Functions

- `main()` - Entry point, parses QUERY_STRING and PATH_INFO
- `serve_file()` - Serves a file download with Content-Disposition
- `url_encode()` / `url_decode()` - URL encoding helpers
- `parse_qs()` - Query string parameter parser
- `fmt_size()` - Human-readable file size formatting

## Known Limitations

- No authentication (public access)
- Hardcoded base path requires recompile to change
- Max path length 1024 chars, max dir depth 64 levels
- No pagination for large directories

## Server Info

- Web server: web
- CGI binary location: /usr/lib/cgi-bin/files.cgi
- fcgiwrap socket: /var/run/fcgiwrap.socket
