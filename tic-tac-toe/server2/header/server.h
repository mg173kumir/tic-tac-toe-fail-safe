#ifndef SERVER_H
#define SERVER_H

#include <QMainWindow>
#include <QObject>
#include <QThread>
#include <QMap>
#include <QVector>
#include <QTcpServer>
#include <QTcpSocket>

namespace Ui {
class Server;
}

class Server : public QMainWindow
{
    Q_OBJECT

public:
    explicit Server(QWidget *parent = 0);
    ~Server();

private:
    Ui::Server *ui;
    // серверные сокеты
    QTcpServer *server_sock;
    QTcpServer *service_sock;    
    QTcpSocket *player1;
    QTcpSocket *player2;
    QTcpSocket *replayer1;
    QTcpSocket *replayer2;
    // порты сервера
    qint16 main_port, serv_port;
    // id, hash
    QMap<quint32, quint32> games_map;
    // младшие 16 бит игровой порт, старишие сервисный
    QMap<QTcpSocket *, quint32> reserv_sr;
    // младшие 32 бита game_id, старшие 32 бита hash
    QMap<QTcpSocket *, quint64> queue_reconnect;
    // собираем треды здесь
    QVector<QThread*> game_threads;

signals:
    void signal_send_reserve(QString, quint32);


public slots:
    virtual
    void slot_new_connect();
    void slot_service_connect();
    void slot_new_reserve(QTcpSocket *reserve);
    void slot_new_player(QTcpSocket *player);
    void slot_reconnect_session(QTcpSocket *player, quint32 game_id, quint16 id,
                        quint32 hash);
    void slot_server_read();
    void slot_reserve_disc();
    void slot_send_reserve(QTcpSocket *dest, QString ip, quint32 ports);
    void slot_add_rcv_ports(QTcpSocket *dest,QString ip, quint16 port);
    void slot_sync_field(quint32 id, quint32 hash);

private slots:
    void on_pushButton_clicked();
    void on_pushButton_4_clicked();
    void on_pushButton_2_clicked();
    void on_pushButton_3_clicked();
};

#endif // SERVER_H
