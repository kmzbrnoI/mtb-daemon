#ifndef DAEMON_SERVER_H
#define DAEMON_SERVER_H

#include <QObject>
#include <QTcpServer>

class DaemonServer : public QObject {
	Q_OBJECT

public:
	DaemonServer(QObject *parent = nullptr);
	void listen(const QHostAddress&, quint16 port);

private slots:
	void serverNewConnection();

private:
	QTcpServer m_server;

signals:
};

#endif
