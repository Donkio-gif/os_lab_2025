#include <getopt.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  int id;
  uint64_t begin; 
  uint64_t end;   
} Range;

static uint64_t mod = 0;
static uint64_t result = 1; 
static pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;

static void *partial_factorial(void *arg) {
  Range *r = (Range *)arg;
  uint64_t local = 1 % mod;

  for (uint64_t i = r->begin; i <= r->end; i++)
    local = (local * (i % mod)) % mod;

  printf("thread %d: [%llu..%llu] partial = %llu\n", r->id,
         (unsigned long long)r->begin, (unsigned long long)r->end,
         (unsigned long long)local);

  pthread_mutex_lock(&mut);
  result = (result * local) % mod;
  pthread_mutex_unlock(&mut);

  return NULL;
}

static int parse_u64(const char *s, uint64_t *out) {
  char *end;
  if (s == NULL || *s == '\0' || *s == '-')
    return 0;
  unsigned long long v = strtoull(s, &end, 10);
  if (*end != '\0')
    return 0;
  *out = (uint64_t)v;
  return 1;
}

static void usage(const char *prog) {
  printf("Usage: %s -k <num> --pnum=<num> --mod=<num>\n", prog);
}

int main(int argc, char **argv) {
  uint64_t k = 0, pnum = 0;
  int have_k = 0, have_pnum = 0, have_mod = 0;

  static struct option options[] = {{"k", required_argument, 0, 'k'},
                                    {"pnum", required_argument, 0, 'p'},
                                    {"mod", required_argument, 0, 'm'},
                                    {0, 0, 0, 0}};

  int c;
  while ((c = getopt_long(argc, argv, "k:", options, NULL)) != -1) {
    switch (c) {
    case 'k':
      if (!parse_u64(optarg, &k)) {
        printf("k must be a non-negative integer\n");
        return 1;
      }
      have_k = 1;
      break;
    case 'p':
      if (!parse_u64(optarg, &pnum) || pnum == 0) {
        printf("pnum must be a positive integer\n");
        return 1;
      }
      have_pnum = 1;
      break;
    case 'm':
      if (!parse_u64(optarg, &mod) || mod == 0 || mod > UINT32_MAX) {
        printf("mod must be an integer in [1, %u]\n", UINT32_MAX);
        return 1;
      }
      have_mod = 1;
      break;
    default:
      usage(argv[0]);
      return 1;
    }
  }

  if (!have_k || !have_pnum || !have_mod) {
    usage(argv[0]);
    return 1;
  }

  uint64_t threads_num = pnum;
  if (k == 0)
    threads_num = 1;
  else if (threads_num > k)
    threads_num = k;

  pthread_t *threads = malloc(threads_num * sizeof(pthread_t));
  Range *ranges = malloc(threads_num * sizeof(Range));
  if (threads == NULL || ranges == NULL) {
    perror("malloc");
    return 1;
  }

  result = 1 % mod;

  uint64_t base = k / threads_num;
  uint64_t rem = k % threads_num;
  uint64_t start = 1;
  for (uint64_t i = 0; i < threads_num; i++) {
    uint64_t len = base + (i < rem ? 1 : 0);
    ranges[i].id = (int)i;
    ranges[i].begin = start;
    ranges[i].end = start + len - 1; 
    start += len;

    if (pthread_create(&threads[i], NULL, partial_factorial, &ranges[i]) != 0) {
      perror("pthread_create");
      return 1;
    }
  }

  for (uint64_t i = 0; i < threads_num; i++) {
    if (pthread_join(threads[i], NULL) != 0) {
      perror("pthread_join");
      return 1;
    }
  }

  printf("%llu! mod %llu = %llu\n", (unsigned long long)k,
         (unsigned long long)mod, (unsigned long long)result);

  free(threads);
  free(ranges);
  return 0;
}