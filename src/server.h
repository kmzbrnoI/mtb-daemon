#ifndef DAEMON_SERVER_H
#define DAEMON_SERVER_H

#include <QObject>
#include <QTcpServer>

struct ServerRequest {
	QTcpSocket* socket;
	std::optional<size_t> id;
};

class DaemonServer : public QObject {
	Q_OBJECT

public:
	DaemonServer(QObject *parent = nullptr);
	void listen(const QHostAddress&, quint16 port);
	void send(QTcpSocket&, const QJsonObject&);

	static QJsonObject error(size_t code, const QString& message);

private slots:
	void serverNewConnection();
	void clientDisconnected();
	void clientReadyRead();

private:
	QTcpServer m_server;

signals:
	void jsonReceived(QTcpSocket*, const QJsonObject&);

};

QJsonObject jsonError(size_t code, const QString& msg);
QJsonObject sendError(QTcpSocket*, const QJsonObject&, size_t code, const QString&);

#endif
