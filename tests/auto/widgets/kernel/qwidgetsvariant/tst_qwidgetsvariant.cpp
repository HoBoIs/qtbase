// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only


#include <QTest>

#include <qvariant.h>

#include "tst_qvariant_common.h"


class tst_QWidgetsVariant : public QObject
{
    Q_OBJECT

private slots:

    void constructor_invalid_data();
    void constructor_invalid();

    void canConvert_data();
    void canConvert();

    void writeToReadFromDataStream_data();
    void writeToReadFromDataStream();

    void qvariant_cast_QObject_data();
    void qvariant_cast_QObject();
    void qvariant_cast_QObject_derived();

    void debugStream_data();
    void debugStream();

    void implicitConstruction();

    void widgetsVariantAtExit();
};

void tst_QWidgetsVariant::constructor_invalid_data()
{
    QTest::addColumn<uint>("typeId");

    QTest::newRow("LastGuiType + 1") << uint(QMetaType::LastGuiType + 1);
    QVERIFY(!QMetaType::isRegistered(QMetaType::LastGuiType + 1));
    QTest::newRow("LastWidgetsType + 1") << uint(QMetaType::LastWidgetsType + 1);
    QVERIFY(!QMetaType::isRegistered(QMetaType::LastWidgetsType + 1));
}

void tst_QWidgetsVariant::constructor_invalid()
{

    QFETCH(uint, typeId);
    {
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("^Trying to construct an instance of an invalid type, type id:"));
        QVariant variant{QMetaType(typeId)};
        QVERIFY(!variant.isValid());
        QCOMPARE(variant.userType(), int(QMetaType::UnknownType));
    }
    {
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("^Trying to construct an instance of an invalid type, type id:"));
        QVariant variant(QMetaType(typeId), nullptr);
        QVERIFY(!variant.isValid());
        QCOMPARE(variant.userType(), int(QMetaType::UnknownType));
    }
}

void tst_QWidgetsVariant::canConvert_data()
{
    TST_QVARIANT_CANCONVERT_DATATABLE_HEADERS

#ifdef Y
#undef Y
#endif
#ifdef N
#undef N
#endif
#define Y true
#define N false

    QVariant var;

    //            bita bitm bool brsh byta col  curs date dt   dbl  font img  int  inv  kseq list ll   map  pal  pen  pix  pnt  rect reg  size sp   str  strl time uint ull


    var = QVariant::fromValue(QSizePolicy());
    QTest::newRow("SizePolicy")
        << var << N << N << N << N << N << N << N << N << N << N << N << N << N << N << N << N << N << N << N << N << N << N << N << N << N << Y << N << N << N << N << N;

#undef N
#undef Y
}

void tst_QWidgetsVariant::canConvert()
{
    TST_QVARIANT_CANCONVERT_FETCH_DATA

    TST_QVARIANT_CANCONVERT_COMPARE_DATA
}


void tst_QWidgetsVariant::writeToReadFromDataStream_data()
{
    QTest::addColumn<QVariant>("writeVariant");
    QTest::addColumn<bool>("isNull");

    QTest::newRow( "sizepolicy_valid" ) << QVariant::fromValue( QSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed ) ) << false;
}

void tst_QWidgetsVariant::writeToReadFromDataStream()
{
    QFETCH( QVariant, writeVariant );
    QFETCH( bool, isNull );
    QByteArray data;

    QDataStream writeStream( &data, QIODevice::WriteOnly );
    writeStream << writeVariant;

    QVariant readVariant;
    QDataStream readStream( &data, QIODevice::ReadOnly );
    readStream >> readVariant;
    QVERIFY( readVariant.isNull() == isNull );
}

class CustomQWidget : public QWidget {
    Q_OBJECT
public:
    CustomQWidget(QWidget *parent = nullptr) : QWidget(parent) {}
};

void tst_QWidgetsVariant::qvariant_cast_QObject_data()
{
    QTest::addColumn<QVariant>("data");
    QTest::addColumn<bool>("success");

    QWidget *widget = new QWidget;
    widget->setObjectName(QString::fromLatin1("Hello"));
    QTest::newRow("from QWidget") << QVariant::fromValue(widget) << true;

    CustomQWidget *customWidget = new CustomQWidget;
    customWidget->setObjectName(QString::fromLatin1("Hello"));
    QTest::newRow("from Derived QWidget") << QVariant::fromValue(customWidget) << true;
}

void tst_QWidgetsVariant::qvariant_cast_QObject()
{
    QFETCH(QVariant, data);
    QFETCH(bool, success);

    QObject *o = qvariant_cast<QObject *>(data);
    QCOMPARE(o != 0, success);
    if (success) {
        QCOMPARE(o->objectName(), QString::fromLatin1("Hello"));
        QVERIFY(data.canConvert<QObject*>());
        QVERIFY(data.canConvert(QMetaType(QMetaType::QObjectStar)));
        QVERIFY(data.canConvert(QMetaType::fromType<QObject*>()));
        QVERIFY(data.value<QObject*>());
        QVERIFY(data.convert(QMetaType(QMetaType::QObjectStar)));
        QCOMPARE(data.metaType().id(), int(QMetaType::QObjectStar));

        QVERIFY(data.canConvert<QWidget*>());
        QVERIFY(data.canConvert(QMetaType::fromType<QWidget*>()));
        QVERIFY(data.value<QWidget*>());
        QVERIFY(data.convert(QMetaType::fromType<QWidget*>()));
        QCOMPARE(data.metaType(), QMetaType::fromType<QWidget*>());
    } else {
        QVERIFY(!data.canConvert<QObject*>());
        QVERIFY(!data.canConvert(QMetaType(QMetaType::QObjectStar)));
        QVERIFY(!data.canConvert(QMetaType::fromType<QObject*>()));
        QVERIFY(!data.value<QObject*>());
        QVERIFY(!data.convert(QMetaType(QMetaType::QObjectStar)));
        QVERIFY(data.metaType().id() != QMetaType::QObjectStar);
    }
    delete o;
}

void tst_QWidgetsVariant::qvariant_cast_QObject_derived()
{
    CustomQWidget customWidget;
    QWidget *widget = &customWidget;
    QVariant data = QVariant::fromValue(widget);
    QCOMPARE(data.userType(), qMetaTypeId<QWidget*>());

    QCOMPARE(data.value<QObject*>(), widget);
    QCOMPARE(data.value<QWidget*>(), widget);
    QCOMPARE(data.value<CustomQWidget*>(), widget);
}

void tst_QWidgetsVariant::debugStream_data()
{
    QTest::addColumn<QVariant>("variant");
    QTest::addColumn<int>("typeId");
    for (int id = QMetaType::LastGuiType + 1; id < QMetaType::User; ++id) {
        const char *tagName = QMetaType(id).name();
        if (!tagName)
            continue;
        QTest::newRow(tagName) << QVariant(QMetaType(id)) << id;
    }
}

void tst_QWidgetsVariant::debugStream()
{
    QFETCH(QVariant, variant);
    QFETCH(int, typeId);

    MessageHandler msgHandler(typeId);
    qDebug() << variant;
    QVERIFY(msgHandler.testPassed());
}

void tst_QWidgetsVariant::widgetsVariantAtExit()
{
    // crash test, it should not crash at QApplication exit
    static QVariant sizePolicy = QSizePolicy();
    Q_UNUSED(sizePolicy);
    QVERIFY(true);
}


void tst_QWidgetsVariant::implicitConstruction()
{
    // This is a compile-time test
    QVariant v;

#define FOR_EACH_WIDGETS_CLASS(F) \
    F(SizePolicy) \

#define CONSTRUCT(TYPE) \
    { \
        Q##TYPE t; \
        v = t; \
        QVERIFY(true); \
    }

    FOR_EACH_WIDGETS_CLASS(CONSTRUCT)

#undef CONSTRUCT
#undef FOR_EACH_WIDGETS_CLASS
}

QTEST_MAIN(tst_QWidgetsVariant)
#include "tst_qwidgetsvariant.moc"
