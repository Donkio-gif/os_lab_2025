#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <errno.h>
#include <getopt.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>

struct Server {
  char ip[255];
  int port;
};

struct ServerTask {
  struct Server server;
  struct sockaddr_in addr; 
  uint64_t begin;
  uint64_t end;
  uint64_t mod;
  uint64_t result; 
  bool ok;         
};

uint64_t MultModulo(uint64_t a, uint64_t b, uint64_t mod) {
  uint64_t result = 0;
  a = a % mod;
  while (b > 0) {
    if (b % 2 == 1)
      result = (result + a) % mod;
    a = (a * 2) % mod;
    b /= 2;
  }

  return result % mod;
}

bool ConvertStringToUI64(const char *str, uint64_t *val) {
  char *end = NULL;
  if (str == NULL || *str == '\0' || *str == '-')
    return false;

  errno = 0;
  unsigned long long i = strtoull(str, &end, 10);
  if (errno == ERANGE) {
    fprintf(stderr, "Out of uint64_t range: %s\n", str);
    return false;
  }

  if (errno != 0 || *end != '\0')
    return false;

  *val = i;
  return true;
}

unsigned int ReadServers(const char *path, struct Server **out) {
  FILE *f = fopen(path, "r");
  if (f == NULL) {
    perror("fopen");
    return 0;
  }

  unsigned int count = 0, capacity = 4;
  struct Server *list = malloc(sizeof(struct Server) * capacity);
  char line[512];

  while (fgets(line, sizeof(line), f) != NULL) {
    line[strcspn(line, "\r\n")] = '\0';
    if (line[0] == '\0')
      continue; 

    struct Server s;
    if (sscanf(line, "%254[^:]:%d", s.ip, &s.port) != 2 || s.port <= 0 ||
        s.port > 65535) {
      fprintf(stderr, "Wrong server line (expected ip:port): %s\n", line);
      continue;
    }

    if (count == capacity) {
      capacity *= 2;
      list = realloc(list, sizeof(struct Server) * capacity);
    }
    list[count++] = s;
  }

  fclose(f);
  *out = list;
  return count;
}

void *WorkWithServer(void *arg) {
  struct ServerTask *t = (struct ServerTask *)arg;
  t->ok = false;

  int sck = socket(AF_INET, SOCK_STREAM, 0);
  if (sck < 0) {
    fprintf(stderr, "Socket creation failed!\n");
    return NULL;
  }

  if (connect(sck, (struct sockaddr *)&t->addr, sizeof(t->addr)) < 0) {
    fprintf(stderr, "Connection to %s:%d failed\n", t->server.ip,
            t->server.port);
    close(sck);
    return NULL;
  }

  char task[sizeof(uint64_t) * 3];
  memcpy(task, &t->begin, sizeof(uint64_t));
  memcpy(task + sizeof(uint64_t), &t->end, sizeof(uint64_t));
  memcpy(task + 2 * sizeof(uint64_t), &t->mod, sizeof(uint64_t));

  if (send(sck, task, sizeof(task), 0) < 0) {
    fprintf(stderr, "Send to %s:%d failed\n", t->server.ip, t->server.port);
    close(sck);
    return NULL;
  }

  char response[sizeof(uint64_t)];
  if (recv(sck, response, sizeof(response), 0) != sizeof(response)) {
    fprintf(stderr, "Receive from %s:%d failed\n", t->server.ip,
            t->server.port);
    close(sck);
    return NULL;
  }

  memcpy(&t->result, response, sizeof(uint64_t));
  t->ok = true;
  printf("%s:%d [%llu..%llu] -> %llu\n", t->server.ip, t->server.port,
         (unsigned long long)t->begin, (unsigned long long)t->end,
         (unsigned long long)t->result);

  close(sck);
  return NULL;
}

int main(int argc, char **argv) {
  uint64_t k = -1;
  uint64_t mod = -1;
  char servers[255] = {'\0'};

  while (true) {
    int current_optind = optind ? optind : 1;

    static struct option options[] = {{"k", required_argument, 0, 0},
                                      {"mod", required_argument, 0, 0},
                                      {"servers", required_argument, 0, 0},
                                      {0, 0, 0, 0}};

    int option_index = 0;
    int c = getopt_long(argc, argv, "", options, &option_index);

    if (c == -1)
      break;

    switch (c) {
    case 0: {
      switch (option_index) {
      case 0:
        if (!ConvertStringToUI64(optarg, &k)) {
          fprintf(stderr, "k must be a non-negative integer\n");
          return 1;
        }
        break;
      case 1:
        if (!ConvertStringToUI64(optarg, &mod) || mod == 0) {
          fprintf(stderr, "mod must be a positive integer\n");
          return 1;
        }
        break;
      case 2:
        if (strlen(optarg) >= sizeof(servers)) {
          fprintf(stderr, "Path to servers file is too long\n");
          return 1;
        }
        strcpy(servers, optarg);
        break;
      default:
        printf("Index %d is out of options\n", option_index);
      }
    } break;

    case '?':
      printf("Arguments error\n");
      break;
    default:
      fprintf(stderr, "getopt returned character code 0%o?\n", c);
    }
  }

  if (k == (uint64_t)-1 || mod == (uint64_t)-1 || !strlen(servers)) {
    fprintf(stderr, "Using: %s --k 1000 --mod 5 --servers /path/to/file\n",
            argv[0]);
    return 1;
  }

  struct Server *to = NULL;
  unsigned int servers_num = ReadServers(servers, &to);
  if (servers_num == 0) {
    fprintf(stderr, "No servers in file %s\n", servers);
    free(to);
    return 1;
  }

  if (k == 0) { 
    printf("answer: %llu\n", (unsigned long long)(1 % mod));
    free(to);
    return 0;
  }

  if (servers_num > k)
    servers_num = (unsigned int)k;

  struct ServerTask *tasks = calloc(servers_num, sizeof(struct ServerTask));
  pthread_t *threads = malloc(sizeof(pthread_t) * servers_num);

  uint64_t base = k / servers_num;
  uint64_t rem = k % servers_num;
  uint64_t start = 1;

  for (unsigned int i = 0; i < servers_num; i++) {
    tasks[i].server = to[i];
    tasks[i].mod = mod;
    tasks[i].begin = start;
    tasks[i].end = start + base + (i < rem ? 1 : 0) - 1;
    start = tasks[i].end + 1;

    struct hostent *hostname = gethostbyname(to[i].ip);
    if (hostname == NULL) {
      fprintf(stderr, "gethostbyname failed with %s\n", to[i].ip);
      exit(1);
    }
    memset(&tasks[i].addr, 0, sizeof(tasks[i].addr));
    tasks[i].addr.sin_family = AF_INET;
    tasks[i].addr.sin_port = htons(to[i].port);
    memcpy(&tasks[i].addr.sin_addr, hostname->h_addr, hostname->h_length);
  }

  for (unsigned int i = 0; i < servers_num; i++) {
    if (pthread_create(&threads[i], NULL, WorkWithServer, &tasks[i]) != 0) {
      perror("pthread_create");
      exit(1);
    }
  }

  uint64_t answer = 1 % mod;
  bool all_ok = true;
  for (unsigned int i = 0; i < servers_num; i++) {
    pthread_join(threads[i], NULL);
    if (!tasks[i].ok)
      all_ok = false;
    else
      answer = MultModulo(answer, tasks[i].result, mod);
  }

  free(threads);
  free(tasks);
  free(to);

  if (!all_ok) {
    fprintf(stderr, "Some servers failed, answer is not complete\n");
    return 1;
  }

  printf("answer: %llu\n", (unsigned long long)answer);
  return 0;
}