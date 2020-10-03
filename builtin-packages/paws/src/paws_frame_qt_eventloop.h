#ifndef __PAWS_FRAME_QT_EVENTLOOP_H
#define __PAWS_FRAME_QT_EVENTLOOP_H

#include <QApplication>
#include <QCoreApplication>
#include <QThread>
#include <QTimer>
#include "paws_frame_eventloop.h"

QApplication* qtApp;

class QtEventLoop : /*public QObject,*/ public IEventLoop
{
private:
	bool running;

public:
	QtEventLoop()
	{
		// moveToThread(QThread::currentThread());
	}
	~QtEventLoop()
	{
		//qApp->deleteLater();
	}
	void run()
	{
		if (!running)
		{
			running = true;
			qApp->exec();
		}
	}
	void close()
	{
		if (running)
		{
			running = false;
			qApp->exit();
		}
	}
	bool idle()
	{
		return true; //TODO
	}
	void post(CALLBACK_TYPE callback, float duration)
	{
		QTimer* timer = new QTimer();
		timer->moveToThread(qApp->thread()); //QThread::currentThread());
		timer->setSingleShot(true);
		QObject::connect(timer, &QTimer::timeout, [=]() {
			callback();
			timer->deleteLater();
		});
		QMetaObject::invokeMethod(timer, "start", Qt::QueuedConnection, Q_ARG(int, 1000 * duration));
	}
};

#endif