#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include "server.h"

DaemonServer::DaemonServer(QObject *parent) : QObject(parent) {
	QObject::connect(&m_server, SIGNAL(newConnection()), this, SLOT(serverNewConnection()));
}

void DaemonServer::listen(const QHostAddress& addr, quint16 port) {
	m_server.listen(addr, port);
}

void DaemonServer::serverNewConnection() {
	QTcpSocket* client = m_server.nextPendingConnection();
	QObject::connect(client, SIGNAL(disconnected()), this, SLOT(clientDisconnected()));
	QObject::connect(client, SIGNAL(readyRead()), this, SLOT(clientReadyRead()));
}

void DaemonServer::clientDisconnected() {
	QTcpSocket* client = static_cast<QTcpSocket*>(QObject::sender());
	client->deleteLater();
}

void DaemonServer::clientReadyRead() {
	QTcpSocket* client = static_cast<QTcpSocket*>(QObject::sender());
	if (client->canReadLine()) {
		QByteArray data = client->readLine();
		if (data.size() > 0) {
			QJsonObject json = QJsonDocument::fromJson(data).object();
			jsonReceived(*client, json);
		}
	}
}

void DaemonServer::send(QTcpSocket& socket, const QJsonObject& jsonObj) {
	socket.write(QJsonDocument(jsonObj).toJson(QJsonDocument::Compact));
	socket.write("\n");
}
