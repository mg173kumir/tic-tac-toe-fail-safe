#ifndef GAME_SESS_H
#define GAME_SESS_H

#include <QObject>
#include <QTcpSocket>
#include <QString>
#include <QVector>
#include <QChar>

class Game_sess : public QObject
{
    Q_OBJECT
private:
    // сокеты игроков
    QTcpSocket *player1;
    QTcpSocket *player2;
    // сокет сервера
    QTcpSocket *listener;
    // уникальный айди
    quint32 game_id;
    // храним игровое поле
    QVector <QChar> game_field;
    // терн - чей ход сейчас
    quint16 size, turn;
    QChar end;

public:
    explicit Game_sess(QTcpSocket *player1, QTcpSocket *player2, quint16 gtype);
    QChar end_game();

signals:
    void signal_sync_field(quint32, quint32);
    void thread_finished();

public slots:
    void data_receive();
    void send_game_id(quint16 id, quint32 game_id);
    void send_message(quint16 id, QString str);
    void send_game_field();
    void send_game_over(quint16 id, quint16 state);
    void check_field(quint16 n_player, quint16 pos);
    void send_reserve(QString reserve_ip, quint16 reserve_port);
};

#endif // GAME_SESS_H
