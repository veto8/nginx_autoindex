# files.cgi - Sortable File Browser

A lightweight C CGI program that provides a sortable web-based file browser for nginx, served via fcgiwrap.

## What It Does

Replaces nginx's built-in `autoindex` with a sortable directory listing. Displays files and folders from `/mnt/it/files/` with:

- **Column sorting** by Name, Size, or Date (click headers to toggle ascending/descending)
- **Breadcrumb navigation** for nested directories
- **Direct file downloads** with correct MIME types
- **Clean URLs** (no query parameters needed)

## URL Structure

```
/                                    -> root listing
/controller/                         -> directory listing
/controller/opensupplier/            -> nested directory listing
/controller/opensupplier/file.csv    -> direct file download
/?sort=name                          -> sort by name
/?sort=size                          -> sort by size
/?sort=date                          -> sort by date
```

## Requirements

- nginx with fcgiwrap
- gcc (to compile)

## Compile

```bash
gcc -O2 -o files.cgi files.cgi.c
```

## Deploy

```bash
scp files.cgi root@<web-server>:/usr/lib/cgi-bin/files.cgi
ssh <web-server> "chmod +x /usr/lib/cgi-bin/files.cgi && systemctl restart fcgiwrap"
```

## Nginx Config

```nginx
location / {
    autoindex on;
    root /mnt/it/files;
    include fastcgi_params;
    fastcgi_pass unix:/var/run/fcgiwrap.socket;
    fastcgi_param SCRIPT_FILENAME /usr/lib/cgi-bin/files.cgi;
    fastcgi_param QUERY_STRING $query_string;
    fastcgi_param PATH_INFO $uri;
}
```

## Files

- `files.cgi.c` - Main CGI source code
- `files.cgi` - Compiled binary
- `copy.sh` - Deploy script to web server

## Configuration

The base directory is defined in `files.cgi.c`:

```c
#define BASE "/mnt/it/files/"
```

Change this path and recompile to serve a different directory.
