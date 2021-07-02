#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include "server.h"
#include "mtbusb/mtbusb.h"
#include "main.h"

DaemonServer::DaemonServer(QObject *parent) : QObject(parent) {
	QObject::connect(&m_server, SIGNAL(newConnection()), this, SLOT(serverNewConnection()));
	QObject::connect(&this->m_tKeepAlive, SIGNAL(timeout()), this, SLOT(tKeepAliveTick()));
}

void DaemonServer::listen(const QHostAddress &addr, quint16 port, bool keepAlive) {
	this->clients.clear();
	if (!m_server.listen(addr, port))
		throw std::logic_error(m_server.errorString().toStdString());

	if (keepAlive)
		this->m_tKeepAlive.start(SERVER_KEEP_ALIVE_SEND_PERIOD_MS);
}

void DaemonServer::serverNewConnection() {
	QTcpSocket *client = m_server.nextPendingConnection();
	log("New client: "+client->peerAddress().toString(), Mtb::LogLevel::Info);
	QObject::connect(client, SIGNAL(disconnected()), this, SLOT(clientDisconnected()));
	QObject::connect(client, SIGNAL(readyRead()), this, SLOT(clientReadyRead()));
	this->clients.insert_or_assign(client, true);
}

void DaemonServer::clientDisconnected() {
	auto client = dynamic_cast<QTcpSocket*>(QObject::sender());
	log("Client disconnected: "+client->peerAddress().toString(), Mtb::LogLevel::Info);
	client->deleteLater();

	if (this->clients.find(client) != this->clients.end())
		this->clients.erase(client);

	clientDisconnected(client);
}

void DaemonServer::clientReadyRead() {
	auto client = dynamic_cast<QTcpSocket*>(QObject::sender());
	while (client->canReadLine()) {
		QByteArray data = client->readLine();
		if (data.size() > 0) {
			QJsonParseError parseError;
			QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
			if (doc.isNull()) {
				log("Invalid json received from client "+client->peerAddress().toString()+"!",
				    Mtb::LogLevel::Warning);
				return;
			}

			QJsonObject json = doc.object();
			try {
				jsonReceived(client, json);
			} catch (const std::logic_error& err) {
				log("Client received data Exception: "+QString(err.what()), Mtb::LogLevel::Error);
			} catch (...) {
				log("Client received data Exception: unknown", Mtb::LogLevel::Error);
			}
		}
	}
}

void DaemonServer::send(QTcpSocket &socket, const QJsonObject &jsonObj) {
	QByteArray data = QJsonDocument(jsonObj).toJson(QJsonDocument::Compact);
	data.push_back('\n');
	socket.write(data);
}

void DaemonServer::send(QTcpSocket *socket, const QJsonObject &jsonObj) {
	if (socket != nullptr)
		this->send(*socket, jsonObj);
}

void DaemonServer::broadcast(const QJsonObject &json) {
	for (const auto &pair : this->clients) {
		QTcpSocket *const socket = pair.first;
		this->send(*socket, json);
	}
}

QJsonObject DaemonServer::error(size_t code, const QString &message) {
	return {{"code", static_cast<int>(code)}, {"message", message}};
}

void DaemonServer::tKeepAliveTick() {
	for (const auto& pair : this->clients)
		this->send(pair.first, {});
}

QJsonObject jsonError(size_t code, const QString &msg) {
	return QJsonObject{
		{"code", static_cast<int>(code)},
		{"message", msg},
	};
}

void sendError(QTcpSocket *socket, const QJsonObject &request, size_t code,
                      const QString& message) {
	QJsonObject response {
		{"command", request["command"]},
		{"type", "response"},
		{"status", "error"},
		{"error", jsonError(code, message)},
	};
	if (request.contains("id"))
		response["id"] = request["id"];
	if (request.contains("address"))
		response["address"] = request["address"];
	server.send(*socket, response);
}

void sendError(QTcpSocket *socket, const QJsonObject &request, Mtb::CmdError cmdError) {
	QJsonObject response {
		{"command", request["command"]},
		{"type", "response"},
		{"status", "error"},
		{"error", jsonError(cmdError)},
	};
	if (request.contains("id"))
		response["id"] = request["id"];
	if (request.contains("address"))
		response["address"] = request["address"];
	server.send(*socket, response);
}

QJsonObject jsonOkResponse(const QJsonObject &request) {
	QJsonObject response{
		{"command", request["command"]},
		{"type", "response"},
		{"status", "ok"},
	};
	if (request.contains("id"))
		response["id"] = request["id"];
	if (request.contains("address"))
		response["address"] = request["address"];
	return response;
}

QJsonObject jsonError(Mtb::CmdError error) {
	return jsonError(static_cast<int>(error)+0x1000, Mtb::cmdErrorToStr(error));
}
