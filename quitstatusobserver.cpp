#include "quitstatusobserver.h"

#include <algorithm>

QuitStatusObserver *QuitStatusObserver::instance;

void quitStatusObserverHandler(int) {
  QuitStatusObserver::getInstance().shouldQuit();
}

QuitStatusObserver::QuitStatusObserver() {}

QuitStatusObserver &QuitStatusObserver::getInstance() {
  if (!instance) {
    instance = new QuitStatusObserver();
  }

  return *instance;
}

void QuitStatusObserver::quit() {
  shouldQuitStatus = true;

  std::lock_guard<std::recursive_mutex> g(quitHandlersMutex);
  for (auto fun : quitHandlers) {
    fun();
  }
}

bool QuitStatusObserver::shouldQuit() const { return shouldQuitStatus; }

void QuitStatusObserver::registerHandler(
    QuitStatusObserver::QuitFunctionHandler_t handler) {
  std::lock_guard<std::recursive_mutex> g(quitHandlersMutex);
  if (std::find(quitHandlers.begin(), quitHandlers.end(), handler) !=
      quitHandlers.end()) {
    quitHandlers.push_back(handler);
  }
}

void QuitStatusObserver::unregister(
    QuitStatusObserver::QuitFunctionHandler_t handler) {
  std::lock_guard<std::recursive_mutex> g(quitHandlersMutex);
  std::remove(quitHandlers.begin(), quitHandlers.end(), handler);
}
