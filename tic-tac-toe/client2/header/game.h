#ifndef GAME_H
#define GAME_H

#include <QMainWindow>
#include <QObject>
#include <QTcpSocket>
#include <QVector>
#include <QMap>
#include <QString>
#include <QChar>

namespace Ui {
class Game;
}

class Game : public QMainWindow
{
    Q_OBJECT

public:
    explicit Game(QWidget *parent = 0);
    ~Game();

private:
    Ui::Game *ui;
    // основной сокет клиента
    QTcpSocket *sock;
    // размер сообщения
    quint16 size;
    // номер игрока
    quint16 id;
    // уникальный айди
    quint32 game_id;
    // данные резервного сервера
    QString reserve_ip;
    quint16 reserve_port;
    // игрове поле
    QVector <QChar> game_field;
    quint16 flag_end;

public slots:
    void start_read();
    void send_message(const QString str);
    void send_turn(quint16 pos);
    void what_end(quint16 flag_end);
    void draw_field();
    void reconnect();

private slots:
    void on_pushButton_clicked();
    void on_pushButton_0_clicked();
    void on_pushButton_1_clicked();
    void on_pushButton_2_clicked();
    void on_pushButton_3_clicked();
    void on_pushButton_4_clicked();
    void on_pushButton_5_clicked();
    void on_pushButton_6_clicked();
    void on_pushButton_7_clicked();
    void on_pushButton_8_clicked();
    void on_pushButton_9_clicked();
    void on_pushButton_10_clicked();
};

#endif // GAME_H
