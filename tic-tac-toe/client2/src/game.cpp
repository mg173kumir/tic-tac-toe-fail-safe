#include <iostream>
#include <QTime>
#include <QDataStream>
#include "game.h"
#include "ui_game.h"

// инициализируем сокет, вносим default данные в форму,
// привязываем сокет к слоту start_read,
// default резерва
Game::Game(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::Game)
{
    ui->setupUi(this);
    sock = new QTcpSocket(this);
    ui->lineEdit_3->setText("127.0.0.1");
    ui->lineEdit_4->setText("7777");
    connect(sock, SIGNAL(readyRead()),
            this, SLOT(start_read()));
    connect(sock, SIGNAL(disconnected()),
            this, SLOT(reconnect()));
    reserve_ip = "";
    reserve_port = 0;
    flag_end = 0;
}

// слот, считывающий сообщения с разными типами
// добавление резервного
void Game::start_read()
{
    QDataStream in(sock);
    size = 0;
    while(true) {
        // Получили размер?
        if (!size) {
            if (sock->bytesAvailable() < (qint64)sizeof(quint16)) {
                break;
            }
            in >> size;
        }
        // Полное сообщение?
        if (sock->bytesAvailable() < size) {
            break;
        }
        quint16 type;
        QTime time;
        QString str;

        in >> type;
        switch(type) {
        // case 10 - сообщение с id игрока
        case 10 :
            in >> id >> game_id;
            break;
        // case 20 - текстовое сообщение
        case 20 :
            in >> time >> str;
            ui->textEdit->append(time.toString() + " " + str);
            break;
        // case 30 - игровое поле
        case 30 :
            in >> game_field;
            draw_field();
            break;
        // case 50 - конец игры
        case 50 :
            in >> flag_end;
            what_end(flag_end);
            sock->disconnectFromHost();
            break;            
        // case 80 - резервный сервер
        case 80 :
            in >> reserve_ip >> reserve_port;
            break;
        // case 120 - отправляем игровое поле и айди игры
        case 120:
            QByteArray array;
            QDataStream out(&array, QIODevice::WriteOnly);
            out << quint16(0) << quint16(130) << game_field << game_id;
            out.device()->seek(0);
            out << quint16(array.size() - sizeof(quint16));
            sock->write(array);
            break;
        }
        size = 0;
    }
}

// отправляем текствое сообщение, аля чат на сервер,
// а он уже отправляет другому игроку
void Game::send_message(const QString str)
{
    QByteArray array;    
    QDataStream out(&array, QIODevice::WriteOnly);
    out << quint16(0) << quint16(20) << QTime::currentTime() << str;
    out.device()->seek(0);
    out << quint16(array.size() - sizeof(quint16));
    sock->write(array);
}

// отправляем свой ход на сервер
void Game::send_turn(quint16 pos)
{
    QByteArray array;
    QDataStream out(&array, QIODevice::WriteOnly);
    out << quint16(0) << quint16(40) << pos;
    out.device()->seek(0);
    out << quint16(array.size() - sizeof(quint16));
    sock->write(array);
}

void Game::what_end(quint16 flag_end)
{
    switch (flag_end) {
    // case 1 - вы победили
    case 1 :
        ui->textEdit->append(QTime::currentTime().toString() +
                             " Server: You win!");
        break;
    // case 2 - вы проиграли
    case 2 :
        ui->textEdit->append(QTime::currentTime().toString() +
                             " Server: You lose!");
        break;
    // case 3 - ничья
    case 3 :
        ui->textEdit->append(QTime::currentTime().toString() +
                             " Server: Standoff!");
        break;
    // case 4 - оппонент вышел
    case 4 :
        ui->textEdit->append(QTime::currentTime().toString() +
                             " Server: Opponent leave!");
        ui->textEdit->append(QTime::currentTime().toString() +
                             " Server: You win!");
        break;
    default:
        break;
    }
}

// отрисовывает игровое поле
void Game::draw_field()
{
    ui->pushButton_0->setText(game_field[0]);
    ui->pushButton_1->setText(game_field[1]);
    ui->pushButton_2->setText(game_field[2]);
    ui->pushButton_3->setText(game_field[3]);
    ui->pushButton_4->setText(game_field[4]);
    ui->pushButton_5->setText(game_field[5]);
    ui->pushButton_6->setText(game_field[6]);
    ui->pushButton_7->setText(game_field[7]);
    ui->pushButton_8->setText(game_field[8]);
}

void Game::reconnect()
{
    if (!flag_end) {
        ui->textEdit->append(QTime::currentTime().toString() +
                              " Client: Reconnect...");
        //sock->disconnectFromHost();
        sock = new QTcpSocket(this);
        connect(sock, SIGNAL(readyRead()),
                this, SLOT(start_read()));
        connect(sock, SIGNAL(disconnected()),
                this, SLOT(reconnect()));


        sock->connectToHost(reserve_ip, reserve_port);
        quint32 hash;
        QByteArray array, field;
        QDataStream out(&array, QIODevice::WriteOnly);
        QDataStream pack(&field, QIODevice::WriteOnly);
        pack << game_field;
        hash = qHash(field);
        out << quint16(0) << quint16(110) << game_id << id << hash;
        out.device()->seek(0);
        out << quint16(array.size() - sizeof(quint16));
        sock->write(array);
    }
}

// кнопка send в чате
// добавляет сообщение в чат
// вызывет слот send_message
void Game::on_pushButton_clicked()
{
    if (ui->lineEdit->text().size()) {
        send_message(ui->lineEdit->text());
        ui->textEdit->append(QTime::currentTime().toString() +
                             " You: " + ui->lineEdit->text());
        ui->lineEdit->clear();
    }
}

// кнопка find game
// подключается к серверу сокет sock
void Game::on_pushButton_9_clicked()
{
    if (sock->ConnectedState == QAbstractSocket::ConnectedState) {
      sock->disconnectFromHost();
      sock = new QTcpSocket(this);
      connect(sock, SIGNAL(readyRead()),
              this, SLOT(start_read()));
      connect(sock, SIGNAL(disconnected()),
              this, SLOT(reconnect()));
    }

    reserve_ip = "";
    reserve_port = 0;
    flag_end = 0;

    ui->textEdit->setText(QTime::currentTime().toString() +
                          " Client: Finding game...");
    sock->connectToHost(ui->lineEdit_3->text(),
                        ui->lineEdit_4->text().toInt());

    QByteArray array;
    QDataStream out(&array, QIODevice::WriteOnly);
    out << quint16(0) << quint16(100);
    out.device()->seek(0);
    out << quint16(array.size() - sizeof(quint16));
    sock->write(array);
}

// кнопка leave
// отключается от сервера
void Game::on_pushButton_10_clicked()
{
    if (sock->ConnectedState == QAbstractSocket::ConnectedState) {
      sock->disconnectFromHost();
      sock = new QTcpSocket(this);
      connect(sock, SIGNAL(readyRead()),
              this, SLOT(start_read()));
      connect(sock, SIGNAL(disconnected()),
              this, SLOT(reconnect()));
      ui->textEdit->setText(QTime::currentTime().toString() +
                            " Client: You are leave...");
    }
}
// ...
Game::~Game()
{
    delete ui;
}

// здесь и далее кнопки игрового поля
void Game::on_pushButton_0_clicked()
{
    send_turn(0);
}

void Game::on_pushButton_1_clicked()
{
    send_turn(1);
}

void Game::on_pushButton_2_clicked()
{
    send_turn(2);
}

void Game::on_pushButton_3_clicked()
{
    send_turn(3);
}

void Game::on_pushButton_4_clicked()
{
    send_turn(4);
}

void Game::on_pushButton_5_clicked()
{
    send_turn(5);
}

void Game::on_pushButton_6_clicked()
{
    send_turn(6);
}

void Game::on_pushButton_7_clicked()
{
    send_turn(7);
}

void Game::on_pushButton_8_clicked()
{
    send_turn(8);
}
