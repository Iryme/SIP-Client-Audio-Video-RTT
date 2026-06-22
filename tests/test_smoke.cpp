#include <QtTest>

class TestSmoke : public QObject { Q_OBJECT
private slots:
    void passes() { QVERIFY(true); }
};

QTEST_GUILESS_MAIN(TestSmoke)
#include "test_smoke.moc"
