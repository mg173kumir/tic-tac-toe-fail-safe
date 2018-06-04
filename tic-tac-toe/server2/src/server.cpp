#include "server.h"
#include "ui_server.h"
#include "game_sess.h"
#include <iostream>
#include <QThread>
#include <QTime>

// инициаилзируем основной и сервисный сокеты
// заполняем поля default информацией
// подключаем сокеты к слоту newConnection
Server::Server(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::Server)
{
    ui->setupUi(this);
    server_sock = new QTcpServer (this);
    service_sock = new QTcpServer(this);    
    player1 = player2 = nullptr;
    replayer1 = replayer2 = nullptr;

    ui->lineEdit->setText("7777");
    ui->lineEdit_2->setText("2222");
    ui->lineEdit_5->setText("localhost");
    ui->lineEdit_4->setText("2222");
    connect(server_sock, SIGNAL(newConnection()),
            this, SLOT(slot_new_connect()));
    connect(service_sock, SIGNAL(newConnection()),
            this, SLOT(slot_service_connect()));    
}

// слот, считывающий сообщения с разными типами
// добавление резервного
void Server::slot_server_read()
{
    auto sen = static_cast<QTcpSocket*>(sender());
    QDataStream in(sen);
    quint16 size = 0;
    while(true) {
        // Получили размер?
        if (!size) {
            if (sen->bytesAvailable() < (qint64)sizeof(quint16)) {
                break;
            }
            in >> size;
        }
        // Полное сообщение?
        if (sen->bytesAvailable() < size) {
            break;
        }
        quint16 type;
        quint16 f = 0;
        quint32 ports;
        quint16 id;
        quint32 hash;
        quint32 game_id;
        QString ip;        
        in >> type;

        switch(type) {
        // case 60 - порты
        case 60 :
            in >> ports;
            // был ли добавлен уже
            for (auto it = reserv_sr.begin(); it != reserv_sr.end(); ++it) {
                if (it.value() == ports)
                    f = 1;
            }
            if (!f) {
                reserv_sr[sen] = ports;

                // обновляем резервный сервер у клиентов
                auto it = reserv_sr.begin();
                if (it != reserv_sr.end())
                    emit signal_send_reserve(
                            it.key()->peerAddress().toString(),
                                         it.value() & 0xFFFF);
            }
            break;
        // case 70 - получен один из списка резервов
        case 70 :
            in >> ip >> ports;
            // был ли добавлен уже
            for (auto it = reserv_sr.begin(); it != reserv_sr.end(); ++it) {
                if (it.value() == ports)
                    f = 1;
            }
            if (!f)
                slot_add_rcv_ports(NULL, ip, ports >> 16);
            break;
        // case 90 - получение игровых полей
        case 90 :
            in >> game_id >> hash;
            if (hash == 0) {
                games_map.erase(games_map.find(game_id));
            } else {
                games_map[game_id] = hash;
            }
            break;
        // case 100 - сообщение о подключении
        case 100 :
            slot_new_player(sen);
            break;
        // case 110 - сообщение о переподключении
        case 110 :
            in >> game_id >> id >> hash;
            slot_reconnect_session(sen, game_id, id, hash);
            break;
        }
        size = 0;
    }
}

// обработка подключения нового резервного сервера
// обратно отсылаем все имеющиеся данные о резервных серверах
void Server::slot_new_reserve(QTcpSocket *reserve)
{
    ui->textEdit_3->append(QTime::currentTime().toString() +
                           " Incoming reserve server: " +
                           reserve->peerAddress().toString());

    // отправляем всех, что знаем
    for (auto it = reserv_sr.begin(); it != reserv_sr.end(); ++it) {
        slot_send_reserve(reserve, it.key()->peerAddress().toString(),
                          it.value());
    }
    // не забываем отправить себя
    slot_add_rcv_ports(reserve, "", 0);

    connect(reserve, SIGNAL(readyRead()),
            this, SLOT(slot_server_read()));
    connect(reserve, SIGNAL(disconnected()),
            this, SLOT(slot_reserve_disc()));

}

// отправляет данные о сервере
// запаковка портов
// по сокету или ip и port
void Server::slot_add_rcv_ports(QTcpSocket *dest, QString ip, quint16 port)
{
    // обычные tcp сокеты
    QTcpSocket *serv;
    serv = new QTcpSocket(this);

    ui->textEdit_3->append(QTime::currentTime().toString() +
                          " Connect to reserve...");

    if(dest == NULL) {
        serv->connectToHost(ip, port);
        connect(serv, SIGNAL(readyRead()),
                this, SLOT(slot_server_read()));
        connect(serv, SIGNAL(disconnected()),
                this, SLOT(slot_reserve_disc()));
    } else
        serv = dest;
    QByteArray array;
    QDataStream out(&array, QIODevice::WriteOnly);
    quint32 ports;
    ports = ui->lineEdit_2->text().toInt();
    ports = (ports << 16) | ui->lineEdit->text().toInt();
    out << quint16(0) << quint16(60) << ports;
    out.device()->seek(0);
    out << quint16(array.size() - sizeof(quint16));
    serv->write(array);
}

// обработка отключения резервного сервера
void Server::slot_reserve_disc()
{
    // от какого сокета это пришло
    auto sen = static_cast<QTcpSocket*>(sender());

    ui->textEdit_3->append(QTime::currentTime().toString() +
                           + " Disconnect reserve server: " +
                           sen->peerAddress().toString());
    // убираем из мап
    reserv_sr.erase(reserv_sr.find(sen));

    // обновляем резервный сервер у клиентов
    auto it = reserv_sr.begin();
    if (it != reserv_sr.end())
        emit signal_send_reserve(it.key()->peerAddress().toString(),
                             it.value() & 0xFFFF);
    else
        emit signal_send_reserve("", 0);
    sen->deleteLater();
}

// получет от игровой сессии состояние поля и синхронизирует его с мап
void Server::slot_sync_field(quint32 id, quint32 hash)
{
    if (hash == 0) {
        games_map.erase(games_map.find(id));
    } else {
        games_map[id] = hash;
    }
    QByteArray array;
    QDataStream out(&array, QIODevice::WriteOnly);
    out << quint16(0) << quint16(90) << id << hash;
    out.device()->seek(0);
    out << quint16(array.size() - sizeof(quint16));
    for (auto it = reserv_sr.begin(); it != reserv_sr.end(); ++it) {
        it.key()->write(array);
    }
}

// отправляет резервный сервер в игровую сессию, а она игрокам
void Server::slot_send_reserve(QTcpSocket *dest, QString ip, quint32 ports)
{
    QByteArray array;
    QDataStream out(&array, QIODevice::WriteOnly);
    out << quint16(0) << quint16(70) << ip << ports;
    out.device()->seek(0);
    out << quint16(array.size() - sizeof(quint16));
    dest->write(array);
}

// обработка подключения нового игорка
// создание игровой сессии
// запихивание ее в отдельный поток
void Server::slot_new_player(QTcpSocket *player)
{
    disconnect(player, 0, 0, 0);
    // определяем номер игрока
    if (player1 == nullptr) {
        player1 = player;        
        // Отвалился? Чистим память.
        connect(player1, SIGNAL(disconnected()),
                player1, SLOT(deleteLater()));
        // Отвалился? Обнулить сокет.
        connect(player1, &QTcpSocket::disconnected, [=] () {
                player1 = nullptr;
        });
        ui->textEdit_3->append(QTime::currentTime().toString()
                               + " Player 1 connected.");

    } else if (player2 == nullptr) {
        player2 = player;
        connect(player2, SIGNAL(disconnected()),
                player2, SLOT(deleteLater()));

        ui->textEdit_3->append(QTime::currentTime().toString()
                               + " Player 2 connected.");
        // создаем тред
        // перекидывем в него все необходмое
        QThread *thread = new QThread(this);
        Game_sess *game_sess = new Game_sess(player1, player2, 0);
        player1->setParent(nullptr);
        player2->setParent(nullptr);
        player1->moveToThread(thread);
        player2->moveToThread(thread);
        game_sess->moveToThread(thread);
        ui->textEdit_3->append(QTime::currentTime().toString()
                               + " Game created.");

        connect(game_sess, &Game_sess::thread_finished,
                 thread, &Game_sess::deleteLater);

        connect(game_sess, &Game_sess::thread_finished,
                 game_sess, &Game_sess::deleteLater);
        // подключаем игровую сессию, чтобы она получала хеш игрового поля
        // от потока сервера
        connect(game_sess, &Game_sess::signal_sync_field,
                this, &Server::slot_sync_field);
        // подключаем игровую сессию к серверу, чтобы она получала
        // резервный сервер
        connect(this, &Server::signal_send_reserve,
                game_sess, &Game_sess::send_reserve);

        thread->start();

        // сразу при создании игр. сессии отправляем данные о доствупном
        // резервном сервере
        auto it = reserv_sr.begin();
        if (it != reserv_sr.end())
            emit signal_send_reserve(it.key()->peerAddress().toString(),
                                 it.value() & 0xFFFF);
        // собираем потоки с играми
        game_threads.push_back(thread);
        player1 = player2 = nullptr;
    }
}

void Server::slot_reconnect_session(QTcpSocket *player, quint32 game_id,
                            quint16 id, quint32 hash)
{
    // отключаем все связанные с сокетом слоты и сигналы
    disconnect(player, 0, 0, 0);
    auto it = queue_reconnect.begin();
    for (it = queue_reconnect.begin();
         it != queue_reconnect.end(); ++it) {
        if ((it.value() & 0xFFFFFFFF) == game_id) {
            if (hash == (it.value() >> 32)) {
                if (id == 1) {
                    replayer1 = player;
                    replayer2 = it.key();
                } else {
                    replayer1 = it.key();
                    replayer2 = player;
                }
                // Отвалился? Чистим память.
                connect(replayer1, SIGNAL(disconnected()),
                        replayer1, SLOT(deleteLater()));
                // Отвалился? Обнулить сокет.
                connect(replayer1, &QTcpSocket::disconnected, [=] () {
                        replayer1 = nullptr;
                });
                ui->textEdit_3->append(QTime::currentTime().toString()
                                       + " Player 1 reconnected.");
                // Отвалился? Чистим память.
                connect(replayer2, SIGNAL(disconnected()),
                        replayer2, SLOT(deleteLater()));
                // Отвалился? Обнулить сокет.
                connect(replayer2, &QTcpSocket::disconnected, [=] () {
                        replayer2 = nullptr;
                });
                ui->textEdit_3->append(QTime::currentTime().toString()
                                       + " Player 2 reconnected.");
                queue_reconnect.erase(it);
                break;
            } else {
                queue_reconnect.erase(it);
                break;
            }
        }
    }

    if (it == queue_reconnect.end()) {
        quint64 buff = hash;
        buff = (buff << 32) | game_id;
        queue_reconnect[player] = buff;
    }

    if (replayer1 != nullptr && replayer2 != nullptr) {

        // создаем тред
        // перекидывем в него все необходмое
        QThread *thread = new QThread(this);
        Game_sess *game_sess = new Game_sess(replayer1, replayer2, 1);
        replayer1->setParent(nullptr);
        replayer2->setParent(nullptr);
        replayer1->moveToThread(thread);
        replayer2->moveToThread(thread);
        game_sess->moveToThread(thread);
        ui->textEdit_3->append(QTime::currentTime().toString()
                               + " Game recreated.");

        connect(game_sess, &Game_sess::thread_finished,
                 game_sess, &Game_sess::deleteLater);
        // подключаем игровую сессию, чтобы она получала хеш игрового поля
        // от потока сервера
        connect(game_sess, &Game_sess::signal_sync_field,
                this, &Server::slot_sync_field);
        // подключаем игровую сессию к серверу, чтобы она получала
        // резервный сервер
        connect(this, &Server::signal_send_reserve,
                game_sess, &Game_sess::send_reserve);

        thread->start();

        // сразу при создании игр. сессии отправляем данные о доствупном
        // резервном сервере
        auto it = reserv_sr.begin();
        if (it != reserv_sr.end())
            emit signal_send_reserve(it.key()->peerAddress().toString(),
                                 it.value() & 0xFFFF);
        // собираем потоки с играми
        game_threads.push_back(thread);
        replayer1 = replayer2 = nullptr;
    }
}

// кнопка start
// запускает два серверных сокета у сервера
void Server::on_pushButton_clicked()
{
    if (!server_sock->listen(QHostAddress::Any,
                             ui->lineEdit->text().toInt())) {
        ui->textEdit_3->setText(QTime::currentTime().toString() +
                                " Start was failed: ");
        server_sock->close();
    }
    if (!service_sock->listen(QHostAddress::Any,
                             ui->lineEdit_2->text().toInt())) {
        ui->textEdit_3->setText(QTime::currentTime().toString() +
                                " Start was failed: ");
        service_sock->close();
    }
    ui->textEdit_3->setText(QTime::currentTime().toString() +
                            " Sever online!");
}

// кнопка connect
// подключаемся к резервному серверу
void Server::on_pushButton_4_clicked()
{
    QString ip = ui->lineEdit_5->text();
    quint16 port = ui->lineEdit_4->text().toInt();
    slot_add_rcv_ports(NULL, ip, port);
}

// кнопка server list
// выводит список серверов
void Server::on_pushButton_2_clicked()
{
    if (reserv_sr.begin() == reserv_sr.end()) {
        ui->textEdit_3->append(QTime::currentTime().toString() +
                               " Res. srv. list are eampty.");
    } else {
        ui->textEdit_3->append(QTime::currentTime().toString() +
                               " Res. srv. list: ");
        for (auto &x : reserv_sr) {
            ui->textEdit_3->append(QString::number(x));
        }
   }
}

// кнопка game list
// выводит список игр, которые играются на этом сервере
void Server::on_pushButton_3_clicked()
{
    if (games_map.begin() == games_map.end()) {
        ui->textEdit_3->append(QTime::currentTime().toString() +
                               " Game list are eampty.");
    } else {
        ui->textEdit_3->append(QTime::currentTime().toString() +
                               " Game list: ");
        for(auto it = games_map.begin(); it != games_map.end(); ++it)
          {
              ui->textEdit_3->append(QString::number(it.key()) + "["
                                     + QString::number(it.value()) + "]");
          }
    }
}

Server::~Server()
{
    delete ui;
    for (auto it : game_threads) {
        it->exit();
    }
}

void Server::slot_new_connect()
{
    connect(server_sock->nextPendingConnection(), SIGNAL(readyRead()),
            this, SLOT(slot_server_read()));
}

void Server::slot_service_connect()
{
    slot_new_reserve(service_sock->nextPendingConnection());
}

