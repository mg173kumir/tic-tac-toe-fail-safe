#include "game_sess.h"
#include <iostream>
#include <QTime>
#include <QString>
#include <QDataStream>
#include <QHash>


Game_sess::Game_sess(QTcpSocket *player1, QTcpSocket *player2, quint16 gtype) :
    player1(player1), player2(player2)
{
    // учим слушать сообщения
    connect(player1, &QTcpSocket::readyRead,
            this, &Game_sess::data_receive);
    connect(player2, &QTcpSocket::readyRead,
            this, &Game_sess::data_receive);

    // обработка побед при отключении
    connect(player1, &QTcpSocket::disconnected, [=] () {
        if (end == QChar(0)) {
            send_game_over(2, 4);
            end = '*';
            emit signal_sync_field(game_id, 0);
            //emit thread_finished();
        }
    });

    // тоже самое
    connect(player2, &QTcpSocket::disconnected, [=] () {
        if (end == QChar(0)) {
            send_game_over(1, 4);
            end = '*';
            emit signal_sync_field(game_id, 0);
            //emit thread_finished();
        }
    });
    if (!gtype) {
        // инициализация
        turn = 1;
        end = QChar(0);
        game_field.reserve(9);
        game_field = {' ',' ',' ',' ',' ',' ',' ',' ',' '};

        // генерируем id игровой сессии
        qsrand(QDateTime::currentMSecsSinceEpoch());
        game_id = qrand() % RAND_MAX;

        // рассылка игрокам
        send_game_field();
        send_game_id(1, game_id);
        send_game_id(2, game_id);
        send_message(1, "Server: Game ready!");
        send_message(1, "Server: Your turn.");
        send_message(2, "Server: Game ready!");
        send_message(2, "Server: Opponent turn.");
    } else {
        // инициализация
        end = QChar(0);
        game_field.reserve(9);
        // сообщаем, что нам нужно поле
        QByteArray array;
        QDataStream out(&array, QIODevice::WriteOnly);
        out << quint16(0) << quint16(120);
        out.device()->seek(0);
        out << quint16(array.size() - sizeof(quint16));
        player1->write(array);
    }
}

// обработка постуипвиших сообщений
void Game_sess::data_receive()
{    
    quint16 n_player = 0;
    if(player1 == (QTcpSocket*)sender()) {
        listener = player1;
        n_player = 1;        
    } else if (player2 == (QTcpSocket*)sender()) {
        listener = player2;
        n_player = 2;
    }

    QDataStream in(listener);
    size = 0;

    while(true) {
        // Получили размер?
        if (!size) {
            if (listener->bytesAvailable() < (qint64)sizeof(quint16)) {
                break;
            }
            in >> size;
        }
        // Полное сообщение?
        if (listener->bytesAvailable() < size) {
            break;
        }
        quint16 ctr = 0;
        quint16 type;
        quint16 pos;
        QString str;
        QTime time;

        in >> type;

        switch(type) {
        // case 40 - ход игрока
        case 40 :
            in >> pos;
            check_field(n_player, pos);
            break;
        // case 20 - текстовое сообщение
        case 20 :
            in >> time >> str;
            if (n_player == 1) {
                send_message(2, "Opponent: " + str);
            } else {
                send_message(1, "Opponent: " + str);
            }
            break;
        // case 130 - получаем данные игровой сессии
        case 130:
            in >> game_field >> game_id;
            for (quint16 i = 0; i < 9; ++i) {
                if (game_field[i] != ' ')
                    ++ctr;
            }
            if (ctr % 2)
                turn = 2;
            else
                turn = 1;
            send_game_field();
            break;
        }
        size = 0;
    }
}

// отправляем игроку уникальный id инровой сессии
void Game_sess::send_game_id(quint16 id, quint32 game_id)
{
    QByteArray array;
    QDataStream out(&array, QIODevice::WriteOnly);
    out << quint16(0) << quint16(10) << id << game_id;
    out.device()->seek(0);
    out << quint16(array.size() - sizeof(quint16));
    if (id == 1) {
        player1->write(array);
    } else if (id == 2) {
        player2->write(array);
    }
}

// отправляется текстовое сообщение игроку
void Game_sess::send_message(quint16 id, QString str)
{
    QByteArray array;
    QDataStream out(&array, QIODevice::WriteOnly);
    out << quint16(0) << quint16(20) << QTime::currentTime() << str;
    out.device()->seek(0);
    out << quint16(array.size() - sizeof(quint16));
    if (id == 1) {
        player1->write(array);
    } else if (id == 2) {
        player2->write(array);
    }
}

// отправка игрокам поле
void Game_sess::send_game_field()
{
    QByteArray array;
    QDataStream out(&array, QIODevice::WriteOnly);
    out << quint16(0) << quint16(30) << game_field;
    out.device()->seek(0);
    out << quint16(array.size() - sizeof(quint16));
    player1->write(array);
    player2->write(array);
}

// сообщает игрокам о конце игры
void Game_sess::send_game_over(quint16 id, quint16 state)
{
    QByteArray array;
    QDataStream out(&array, QIODevice::WriteOnly);
    out << quint16(0) << quint16(50) << state;
    out.device()->seek(0);
    out << quint16(array.size() - sizeof(quint16));
    if (id == 1)
        player1->write(array);
    else
        player2->write(array);
}

// отправить игрокам новый резервный сервер
void Game_sess::send_reserve(QString reserve_ip, quint16 reserve_port)
{
    if (end == QChar(0)) {
        QByteArray array;
        QDataStream out(&array, QIODevice::WriteOnly);
        out << quint16(0) << quint16(80) << reserve_ip << reserve_port;
        out.device()->seek(0);
        out << quint16(array.size() - sizeof(quint16));
        player1->write(array);
        player2->write(array);
    }
}

// проверка корректности хода
// считаем хэш
void Game_sess::check_field(quint16 n_player, quint16 pos)
{
    QByteArray array;
    QDataStream pack(&array, QIODevice::WriteOnly);
    quint32 hash;
    if (n_player == 1 && turn == 1 && game_field[pos] == ' '
            && end == QChar(0)) {
        game_field[pos] = 'X';
        send_game_field();
        end = end_game();
        if (end == QChar(0)) {
            send_message(1, "Server: Opponent turn.");
            send_message(2, "Server: You turn.");
        }
        pack << game_field;
        hash = qHash(array);
        emit signal_sync_field(game_id, hash);
        turn = 2;
    } else if (n_player == 2 && turn == 2 && game_field[pos] == ' '
               && end == QChar(0)){
        game_field[pos] = 'O';
        send_game_field();
        end = end_game();        
        pack << game_field;
        hash = qHash(array);
        if (end == QChar(0)) {
            send_message(1, "Server: You turn.");
            send_message(2, "Server: Opponent turn.");
            emit signal_sync_field(game_id, hash);
        }
        turn = 1;
    }
    if (end == 'X') {
        send_game_over(1, 1);
        send_game_over(2, 2);
        emit signal_sync_field(game_id, 0);
    } else if (end == 'O') {
        send_game_over(1, 2);
        send_game_over(2, 1);
        emit signal_sync_field(game_id, 0);
    } else if (end == '=') {
        send_game_over(1, 3);
        send_game_over(2, 3);
        emit signal_sync_field(game_id, 0);
    }
}

// проверка конца игры
QChar Game_sess::end_game()
{
    // Проверка поля на конец игры:
    // Выйгрышные кейсы:
    if (game_field[0] == game_field[1] && game_field[1] == game_field[2] &&
            game_field[0] != ' ')
        return game_field[0];
    if (game_field[3] == game_field[4] && game_field[4] == game_field[5] &&
            game_field[3] != ' ')
        return game_field[3];
    if (game_field[6] == game_field[7] && game_field[6] == game_field[8] &&
            game_field[6] != ' ')
        return game_field[6];
    if (game_field[0] == game_field[3] && game_field[0] == game_field[6] &&
            game_field[0] != ' ')
        return game_field[0];
    if (game_field[1] == game_field[4] && game_field[1] == game_field[7] &&
            game_field[1] != ' ')
        return game_field[1];
    if (game_field[2] == game_field[5] && game_field[2] == game_field[8] &&
            game_field[2] != ' ')
        return game_field[2];
    if (game_field[0] == game_field[4] && game_field[0] == game_field[8] &&
            game_field[0] != ' ')
        return game_field[0];
    if (game_field[2] == game_field[4] && game_field[2] == game_field[6] &&
            game_field[2] != ' ')
        return game_field[2];

    // Проверка на ничью.
    quint16 ctr = 0;
    for (quint16 i = 0; i < 9; ++i) {
        if (game_field[i] != ' ')
            ++ctr;
    }
    if (ctr == 9) {
        return '=';
    }
    return 0;
}
