// [Blur2PC] Darwin counting semaphore and Win32-compatible automatic-reset event state.
#ifndef BD_NATIVE_COUNT_STATE_H
#define BD_NATIVE_COUNT_STATE_H
#include <pthread.h>
#include <new>
struct bdNativeCountState {
 pthread_mutex_t mutex;
 pthread_cond_t condition;
 unsigned count,maxCount,waiters;
 bool destroyed;
 static bdNativeCountState* create(unsigned initial,unsigned maximum) {
  if(!maximum || initial>maximum)return nullptr;
  auto* state=new(std::nothrow) bdNativeCountState;
  if(!state)return nullptr;
  if(pthread_mutex_init(&state->mutex,nullptr)!=0){delete state;return nullptr;}
  if(pthread_cond_init(&state->condition,nullptr)!=0){pthread_mutex_destroy(&state->mutex);delete state;return nullptr;}
  state->count=initial;state->maxCount=maximum;state->waiters=0;state->destroyed=false;return state;
 }
 static void destroy(bdNativeCountState*& state) {
  if(!state)return;
  pthread_mutex_lock(&state->mutex);state->destroyed=true;pthread_cond_broadcast(&state->condition);
  while(state->waiters) pthread_cond_wait(&state->condition,&state->mutex);
  pthread_mutex_unlock(&state->mutex);pthread_cond_destroy(&state->condition);pthread_mutex_destroy(&state->mutex);
  delete state;state=nullptr;
 }
 static void release(bdNativeCountState* state) {
  if(!state)return;
  pthread_mutex_lock(&state->mutex);
  if(!state->destroyed && state->count<state->maxCount){++state->count;pthread_cond_signal(&state->condition);}
  pthread_mutex_unlock(&state->mutex);
 }
 static bool wait(bdNativeCountState* state) {
  if(!state)return false;
  pthread_mutex_lock(&state->mutex);++state->waiters;
  int status=0;
  while(!state->destroyed && state->count==0 && status==0)status=pthread_cond_wait(&state->condition,&state->mutex);
  const bool signaled=!state->destroyed && status==0;
  if(signaled)--state->count;
  --state->waiters;if(state->destroyed)pthread_cond_broadcast(&state->condition);
  pthread_mutex_unlock(&state->mutex);return signaled;
 }
};
#endif
