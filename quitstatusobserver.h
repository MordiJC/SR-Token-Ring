#ifndef QUITSTATUSOBSERVER_H
#define QUITSTATUSOBSERVER_H

#include <atomic>
#include <csignal>
#include <memory>
#include <vector>
#include <mutex>

void quitStatusObserverHandler(int);

class QuitStatusObserver {
public:
  typedef void(*QuitFunctionHandler_t)();
 private:
  static std::unique_ptr<QuitStatusObserver> instance;
  std::atomic_bool shouldQuitStatus{false};

  std::vector<QuitFunctionHandler_t> quitHandlers;

  std::recursive_mutex quitHandlersMutex;

  QuitStatusObserver();
  QuitStatusObserver(const QuitStatusObserver &) = delete;
  QuitStatusObserver(QuitStatusObserver &) = delete;
  QuitStatusObserver &operator=(const QuitStatusObserver &) = delete;
  QuitStatusObserver &operator=(QuitStatusObserver &) = delete;

 public:
  static QuitStatusObserver &getInstance();

  void quit();

  bool shouldQuit() const;

  void registerHandler(QuitFunctionHandler_t handler);

  void unregister(QuitFunctionHandler_t handler);
};

#endif  // QUITSTATUSOBSERVER_H
