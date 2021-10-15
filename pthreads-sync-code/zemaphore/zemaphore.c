#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <wait.h>
#include "zemaphore.h"

void zem_init(zem_t *s, int value) {
  pthread_mutex_lock(&s->mutex);
  s->value = value;
  pthread_mutex_unlock(&s->mutex);
}

void zem_down(zem_t *s) {
    pthread_mutex_lock(&s->mutex);
    s->value--;

    if (s->value < 0){
        pthread_cond_wait(&s->cv, &s->mutex);
    }

    pthread_mutex_unlock(&s->mutex);
}

void zem_up(zem_t *s) {
  pthread_mutex_lock(&s->mutex);
  s->value++;
  pthread_cond_signal(&s->cv);
  pthread_mutex_unlock(&s->mutex);
}
