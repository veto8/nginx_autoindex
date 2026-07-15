#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

#define BASE "/mnt/it/files/"

static char cur_dir[512] = "";
static char g_sort[32] = "date";

void url_decode(char *dst, const char *src) {
    char *p = dst;
    while (*src) {
        if (*src == '%' && src[1] && src[2]) {
            char hex[3] = { src[1], src[2], 0 };
            *p++ = (char)strtol(hex, NULL, 16);
            src += 3;
        } else if (*src == '+') {
            *p++ = ' ';
            src++;
        } else {
            *p++ = *src++;
        }
    }
    *p = 0;
}

char *url_encode(const char *src) {
    size_t len = strlen(src);
    char *out = malloc(len * 3 + 1);
    char *p = out;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = src[i];
        if (c == '/' || c == ' ' || c == '&' || c == '?' || c == '%' || c < 33 || c > 126)
            p += sprintf(p, "%%%02X", c);
        else
            *p++ = c;
    }
    *p = 0;
    return out;
}

void parse_qs(const char *qs, const char *key, char *out, size_t max) {
    out[0] = 0;
    if (!qs) return;
    size_t klen = strlen(key);
    const char *p = qs;
    while ((p = strstr(p, key)) != NULL) {
        if (p == qs || *(p - 1) == '&') {
            p += klen;
            size_t i = 0;
            while (p[i] && p[i] != '&' && i < max - 1) { out[i] = p[i]; i++; }
            out[i] = 0;
            return;
        }
        p++;
    }
}

void fmt_size(char *buf, size_t sz, off_t b) {
    const char *u[] = {"B","KB","MB","GB","TB"};
    int i = 0;
    double s = b;
    while (s >= 1024 && i < 4) { s /= 1024; i++; }
    sprintf(buf, "%.1f %s", s, u[i]);
}

/* entries array with precomputed stat */
typedef struct {
    char *name;
    off_t size;
    time_t mtime;
    int is_dir;
    char size_str[32];
    char time_str[64];
} entry_t;

int cmp_date(const void *a, const void *b) {
    return (int)difftime(((entry_t*)b)->mtime, ((entry_t*)a)->mtime);
}
int cmp_date_r(const void *a, const void *b) {
    return (int)difftime(((entry_t*)a)->mtime, ((entry_t*)b)->mtime);
}
int cmp_name(const void *a, const void *b) {
    return strcasecmp(((entry_t*)a)->name, ((entry_t*)b)->name);
}
int cmp_name_r(const void *a, const void *b) {
    return strcasecmp(((entry_t*)b)->name, ((entry_t*)a)->name);
}
int cmp_size(const void *a, const void *b) {
    off_t d = ((entry_t*)a)->size - ((entry_t*)b)->size;
    return d > 0 ? 1 : d < 0 ? -1 : 0;
}
int cmp_size_r(const void *a, const void *b) {
    off_t d = ((entry_t*)b)->size - ((entry_t*)a)->size;
    return d > 0 ? 1 : d < 0 ? -1 : 0;
}

void serve_file(const char *name) {
    char decoded[512];
    url_decode(decoded, name);
    char path[1024];
    snprintf(path, sizeof(path), "%s%s/%s", BASE, cur_dir, decoded);

    struct stat st;
    if (stat(path, &st) < 0 || S_ISDIR(st.st_mode)) {
        printf("Status: 404 Not Found\r\nContent-Type: text/plain\r\n\r\nNot found.\n");
        return;
    }

    const char *ext = strrchr(decoded, '.');
    const char *ctype = "application/octet-stream";
    if (ext) {
        if (!strcasecmp(ext, ".html")) ctype = "text/html";
        else if (!strcasecmp(ext, ".css")) ctype = "text/css";
        else if (!strcasecmp(ext, ".js")) ctype = "application/javascript";
        else if (!strcasecmp(ext, ".json")) ctype = "application/json";
        else if (!strcasecmp(ext, ".png")) ctype = "image/png";
        else if (!strcasecmp(ext, ".jpg") || !strcasecmp(ext, ".jpeg")) ctype = "image/jpeg";
        else if (!strcasecmp(ext, ".gif")) ctype = "image/gif";
        else if (!strcasecmp(ext, ".pdf")) ctype = "application/pdf";
        else if (!strcasecmp(ext, ".zip")) ctype = "application/zip";
        else if (!strcasecmp(ext, ".txt") || !strcasecmp(ext, ".log")) ctype = "text/plain";
        else if (!strcasecmp(ext, ".csv")) ctype = "text/csv";
        else if (!strcasecmp(ext, ".xml")) ctype = "text/xml";
    }

    printf("Content-Type: %s\r\n", ctype);
    printf("Content-Disposition: attachment; filename=\"%s\"\r\n", decoded);
    printf("Content-Length: %ld\r\n\r\n", (long)st.st_size);

    FILE *f = fopen(path, "rb");
    if (!f) return;
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        fwrite(buf, 1, n, stdout);
    fclose(f);
}

int main(void) {
    char *qs = getenv("QUERY_STRING");
    if (!qs) qs = "";

    /* parse params */
    char raw_dir[512], raw_file[512], raw_sort[32];
    parse_qs(qs, "dir=", raw_dir, sizeof(raw_dir));
    parse_qs(qs, "file=", raw_file, sizeof(raw_file));
    parse_qs(qs, "sort=", raw_sort, sizeof(raw_sort));

    url_decode(cur_dir, raw_dir);

    /* if no dir= in query, use PATH_INFO */
    if (!cur_dir[0]) {
        char *pi = getenv("PATH_INFO");
        if (pi && pi[0] && pi[0] != '/') {
            strncpy(cur_dir, pi, sizeof(cur_dir) - 1);
        } else if (pi && pi[1]) {
            strncpy(cur_dir, pi + 1, sizeof(cur_dir) - 1);
        }
        /* strip trailing slash */
        size_t clen = strlen(cur_dir);
        if (clen > 0 && cur_dir[clen - 1] == '/')
            cur_dir[clen - 1] = 0;
    }

    /* check if path is a file (not a directory) - serve it directly */
    {
        char checkpath[1024];
        snprintf(checkpath, sizeof(checkpath), "%s%s", BASE, cur_dir);
        struct stat st;
        if (stat(checkpath, &st) == 0 && !S_ISDIR(st.st_mode)) {
            /* extract filename from path */
            const char *fname = strrchr(cur_dir, '/');
            if (fname) fname++;
            else fname = cur_dir;
            /* set cur_dir to parent directory */
            char *slash = strrchr(cur_dir, '/');
            if (slash) *slash = 0;
            else cur_dir[0] = 0;
            serve_file(fname);
            return 0;
        }
    }

    /* file download via ?file= */
    if (raw_file[0]) {
        serve_file(raw_file);
        return 0;
    }

    /* sort: validate and copy */
    if (!strcmp(raw_sort, "name") || !strcmp(raw_sort, "name_r") ||
        !strcmp(raw_sort, "size") || !strcmp(raw_sort, "size_r") ||
        !strcmp(raw_sort, "date_r"))
        strcpy(g_sort, raw_sort);
    else
        strcpy(g_sort, "date");

    /* scandir with NULL comparator (no sorting) */
    char fullpath[1024];
    snprintf(fullpath, sizeof(fullpath), "%s%s", BASE, cur_dir[0] ? cur_dir : "");

    struct dirent **list;
    int n = scandir(fullpath, &list, NULL, NULL);
    if (n < 0) {
        printf("Content-Type: text/html\r\n\r\n<html><body><h2>Cannot open directory</h2></body></html>");
        return 1;
    }

    /* build entry array */
    entry_t *entries = malloc(sizeof(entry_t) * n);
    int count = 0;
    for (int i = 0; i < n; i++) {
        if (!strcmp(list[i]->d_name, ".") || !strcmp(list[i]->d_name, "..")) {
            free(list[i]);
            continue;
        }
        char path[1024];
        struct stat st;
        snprintf(path, sizeof(path), "%s/%s", fullpath, list[i]->d_name);
        if (stat(path, &st) < 0) { free(list[i]); continue; }

        entries[count].name = strdup(list[i]->d_name);
        entries[count].size = st.st_size;
        entries[count].mtime = st.st_mtime;
        entries[count].is_dir = S_ISDIR(st.st_mode);
        fmt_size(entries[count].size_str, sizeof(entries[count].size_str), st.st_size);
        strftime(entries[count].time_str, sizeof(entries[count].time_str),
                 "%Y-%m-%d %H:%M", localtime(&st.st_mtime));
        count++;
    }

    /* sort with qsort */
    int reverse = 0;
    int is_default = (raw_sort[0] == 0); /* no sort param = default */
    if (!strcmp(g_sort, "name_r")) { strcpy(g_sort, "name"); reverse = 1; }
    else if (!strcmp(g_sort, "size_r")) { strcpy(g_sort, "size"); reverse = 1; }
    else if (!strcmp(g_sort, "date_r")) { strcpy(g_sort, "date"); reverse = 1; }

    if (!strcmp(g_sort, "name"))
        qsort(entries, count, sizeof(entry_t), reverse ? cmp_name : cmp_name_r);
    else if (!strcmp(g_sort, "size"))
        qsort(entries, count, sizeof(entry_t), reverse ? cmp_size : cmp_size_r);
    else if (is_default)
        qsort(entries, count, sizeof(entry_t), cmp_date);       /* newest first */
    else
        qsort(entries, count, sizeof(entry_t), reverse ? cmp_date : cmp_date_r);

    /* next click: first = desc, second = asc, etc */
    const char *next_name = "name_r", *next_size = "size_r", *next_date = "date_r";
    if (!strcmp(g_sort, "name")) next_name = "name";
    else if (!strcmp(g_sort, "size")) next_size = "size";
    else next_date = "date";

    /* output */
    const char *host = getenv("HTTP_HOST");
    if (!host) host = "localhost";
    const char *proto = "http";
    const char *https = getenv("HTTPS");
    const char *fwd = getenv("HTTP_X_FORWARDED_PROTO");
    if ((https && strcmp(https, "off")) || (fwd && !strcmp(fwd, "https")))
        proto = "https";
    char base_url[1024];
    snprintf(base_url, sizeof(base_url), "%s://%s", proto, host);

    printf("Content-Type: text/html\r\n\r\n");
    printf("<html><head><title>Files</title></head><body>");

    printf("<h2>");
    printf("<a href='%s/'>/mnt/it/files</a>", base_url);
    if (cur_dir[0]) {
        char tmp[512];
        strncpy(tmp, cur_dir, sizeof(tmp));
        char *parts[64];
        int np = 0;
        char *tok = strtok(tmp, "/");
        while (tok && np < 64) { parts[np++] = tok; tok = strtok(NULL, "/"); }
        char acc[512] = "";
        for (int i = 0; i < np; i++) {
            if (acc[0]) strcat(acc, "/");
            strcat(acc, parts[i]);
            printf(" / <a href='%s/%s/'>%s</a>", base_url, acc, parts[i]);
        }
    }
    printf("</h2>");

    char *enc_d = url_encode(cur_dir);
    char prefix[1024];
    if (cur_dir[0])
        snprintf(prefix, sizeof(prefix), "/%s", cur_dir);
    else
        snprintf(prefix, sizeof(prefix), "");

    printf("<table border='1' cellpadding='4'>"
           "<tr>"
           "<th><a href='%s/%s/%s%s'>Name %s</a></th>"
           "<th><a href='%s/%s/%s%s'>Size %s</a></th>"
           "<th><a href='%s/%s/%s%s'>Date %s</a></th>"
           "</tr>",
           base_url, cur_dir, cur_dir[0]?"/":"", next_name,
           !strcmp(g_sort,"name") ? (reverse?"&#9660;":"&#9650;") : "",
           base_url, cur_dir, cur_dir[0]?"/":"", next_size,
           !strcmp(g_sort,"size") ? (reverse?"&#9660;":"&#9650;") : "",
           base_url, cur_dir, cur_dir[0]?"/":"", next_date,
           (is_default || !strcmp(g_sort,"date")) ? (is_default?"&#9660;":(reverse?"&#9660;":"&#9650;")) : "");

    for (int i = 0; i < count; i++) {
        if (entries[i].is_dir) {
            char newdir[1024];
            if (cur_dir[0])
                snprintf(newdir, sizeof(newdir), "%s/%s", cur_dir, entries[i].name);
            else
                snprintf(newdir, sizeof(newdir), "%s", entries[i].name);
            printf("<tr><td>[DIR] <a href='%s/%s/'>%s</a></td><td>%s</td><td>%s</td></tr>",
                   base_url, newdir, entries[i].name, entries[i].size_str, entries[i].time_str);
        } else {
            char *enc_f = url_encode(entries[i].name);
            printf("<tr><td><a href='%s/%s/%s'>%s</a></td><td>%s</td><td>%s</td></tr>",
                   base_url, cur_dir, enc_f, entries[i].name, entries[i].size_str, entries[i].time_str);
            free(enc_f);
        }
    }
    printf("</table></body></html>");

    for (int i = 0; i < count; i++) free(entries[i].name);
    free(entries);
    free(list);
    return 0;
}
