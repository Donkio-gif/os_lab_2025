#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static pthread_mutex_t mutex_a = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mutex_b = PTHREAD_MUTEX_INITIALIZER;
static int fix = 0;

static void *thread1_func(void *arg) {
  (void)arg;
  pthread_mutex_lock(&mutex_a);
  printf("thread 1: locked A, waiting for B...\n");
  sleep(1);

  pthread_mutex_lock(&mutex_b);
  printf("thread 1: locked B, working\n");

  pthread_mutex_unlock(&mutex_b);
  pthread_mutex_unlock(&mutex_a);
  printf("thread 1: done\n");
  return NULL;
}

static void *thread2_func(void *arg) {
  (void)arg;
  pthread_mutex_t *first = fix ? &mutex_a : &mutex_b;
  pthread_mutex_t *second = fix ? &mutex_b : &mutex_a;
  const char *first_name = fix ? "A" : "B";
  const char *second_name = fix ? "B" : "A";

  pthread_mutex_lock(first);
  printf("thread 2: locked %s, waiting for %s...\n", first_name, second_name);
  sleep(1);

  pthread_mutex_lock(second);
  printf("thread 2: locked %s, working\n", second_name);

  pthread_mutex_unlock(second);
  pthread_mutex_unlock(first);
  printf("thread 2: done\n");
  return NULL;
}

int main(int argc, char **argv) {
  pthread_t t1, t2;

  setvbuf(stdout, NULL, _IONBF, 0); /* выводить сразу, без буферизации */

  if (argc > 1 && strcmp(argv[1], "fix") == 0)
    fix = 1;

  printf("mode: %s\n", fix ? "fix (same lock order)" : "deadlock");

  if (pthread_create(&t1, NULL, thread1_func, NULL) != 0 ||
      pthread_create(&t2, NULL, thread2_func, NULL) != 0) {
    perror("pthread_create");
    exit(1);
  }

  pthread_join(t1, NULL);
  pthread_join(t2, NULL);

  printf("main: all threads finished\n");
  return 0;
}