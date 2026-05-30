#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUF_SIZE 4096

/* Build a RESP command from space-separated tokens.
   e.g. "SET foo bar" -> "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n" */
static int build_resp(const char *line, char *out, size_t out_size) {
    /* Tokenise */
    char copy[BUF_SIZE];
    snprintf(copy, sizeof(copy), "%s", line);

    /* Strip trailing newline */
    size_t len = strlen(copy);
    while (len > 0 && (copy[len-1] == '\n' || copy[len-1] == '\r'))
        copy[--len] = '\0';
    if (len == 0) return 0;

    char *tokens[64];
    int   ntok = 0;
    char *tok  = strtok(copy, " ");
    while (tok && ntok < 64) {
        tokens[ntok++] = tok;
        tok = strtok(NULL, " ");
    }
    if (ntok == 0) return 0;

    int pos = 0;
    pos += snprintf(out + pos, out_size - pos, "*%d\r\n", ntok);
    for (int i = 0; i < ntok; i++)
        pos += snprintf(out + pos, out_size - pos, "$%zu\r\n%s\r\n",
                        strlen(tokens[i]), tokens[i]);
    return pos;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <host> <port>\n", argv[0]);
        return 1;
    }

    const char *host = argv[1];
    int port = atoi(argv[2]);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return 1; }

    struct sockaddr_in addr = {0};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);
    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid host: %s\n", host); return 1;
    }

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect"); return 1;
    }
    printf("Connected to %s:%d\n", host, port);

    char input[BUF_SIZE];
    char resp[BUF_SIZE];
    char reply[BUF_SIZE];

    while (1) {
        printf("%s:%d> ", host, port);
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin)) break;

        /* Local exit */
        char trimmed[BUF_SIZE];
        snprintf(trimmed, sizeof(trimmed), "%s", input);
        size_t l = strlen(trimmed);
        while (l > 0 && (trimmed[l-1] == '\n' || trimmed[l-1] == '\r'))
            trimmed[--l] = '\0';
        if (strcasecmp(trimmed, "exit") == 0 || strcasecmp(trimmed, "quit") == 0) {
            /* Send QUIT to server */
            int n = build_resp("QUIT", resp, sizeof(resp));
            if (n > 0 && write(fd, resp, n) < 0) perror("write");
            break;
        }

        int n = build_resp(input, resp, sizeof(resp));
        if (n <= 0) continue;

        if (write(fd, resp, n) < 0) { perror("write"); break; }

        ssize_t rnum = read(fd, reply, sizeof(reply) - 1);
        if (rnum <= 0) { printf("Connection closed.\n"); break; }
        reply[rnum] = '\0';

        /* Pretty-print the raw RESP reply */
        printf("%s", reply);
        if (reply[rnum-1] != '\n') putchar('\n');
    }

    close(fd);
    return 0;
}
