// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qmimedata.h"

#include "private/qobject_p.h"
#include "qurl.h"
#include "qstringlist.h"
#include "qstringconverter.h"

QT_BEGIN_NAMESPACE

using namespace Qt::StringLiterals;

static inline QString textUriListLiteral() { return u"text/uri-list"_s; }
static inline QString textHtmlLiteral() { return u"text/html"_s; }
static inline QString textPlainLiteral() { return u"text/plain"_s; }
static inline QString textPlainUtf8Literal() { return u"text/plain;charset=utf-8"_s; }
static inline QString applicationXColorLiteral() { return u"application/x-color"_s; }
static inline QString applicationXQtImageLiteral() { return u"application/x-qt-image"_s; }

struct QMimeDataStruct
{
    QString format;
    QVariant data;
};
Q_DECLARE_TYPEINFO(QMimeDataStruct, Q_RELOCATABLE_TYPE);

class QMimeDataPrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QMimeData)
public:
    void removeData(const QString &format);
    void setData(const QString &format, const QVariant &data);
    QVariant getData(const QString &format) const;

    QVariant retrieveTypedData(const QString &format, QMetaType type) const;

    std::vector<QMimeDataStruct>::iterator find(const QString &format) noexcept {
        const auto formatEquals = [](const QString &format) {
            return [&format](const QMimeDataStruct &s) { return s.format == format; };
        };
        return std::find_if(dataList.begin(), dataList.end(), formatEquals(format));
    }

    std::vector<QMimeDataStruct>::const_iterator find(const QString &format) const noexcept {
        return const_cast<QMimeDataPrivate*>(this)->find(format);
    }

    std::vector<QMimeDataStruct> dataList;
};

void QMimeDataPrivate::removeData(const QString &format)
{
    const auto it = find(format);
    if (it != dataList.end())
        dataList.erase(it);
}

void QMimeDataPrivate::setData(const QString &format, const QVariant &data)
{
    const auto it = find(format);
    if (it == dataList.end())
        dataList.push_back({format, data});
    else
        it->data = data;
}


QVariant QMimeDataPrivate::getData(const QString &format) const
{
    const auto it = find(format);
    if (it == dataList.cend())
        return {};
    else
        return it->data;
}

static QList<QVariant> dataToUrls(QByteArrayView text)
{
    QList<QVariant> list;
    qsizetype newLineIndex = -1;
    qsizetype from = 0;
    const char *begin = text.data();
    while ((newLineIndex = text.indexOf('\n', from)) != -1) {
        const auto bav = QByteArrayView(begin + from, begin + newLineIndex).trimmed();
        if (!bav.isEmpty())
            list.push_back(QUrl::fromEncoded(bav));
        from = newLineIndex + 1;
        if (from >= text.size())
            break;
    }
    if (from != text.size()) {
        const auto bav = QByteArrayView(begin + from, text.end()).trimmed();
        if (!bav.isEmpty())
            list.push_back(QUrl::fromEncoded(bav));
    }
    return list;
}

QVariant QMimeDataPrivate::retrieveTypedData(const QString &format, QMetaType type) const
{
    Q_Q(const QMimeData);
    int typeId = type.id();

    QVariant data = q->retrieveData(format, type);

    // Text data requested: fallback to URL data if available
    if (format == "text/plain"_L1 && !data.isValid()) {
        data = retrieveTypedData(textUriListLiteral(), QMetaType(QMetaType::QVariantList));
        if (data.metaType().id() == QMetaType::QUrl) {
            data = QVariant(data.toUrl().toDisplayString());
        } else if (data.metaType().id() == QMetaType::QVariantList) {
            QString text;
            int numUrls = 0;
            const QList<QVariant> list = data.toList();
            for (const auto &element : list) {
                if (element.metaType().id() == QMetaType::QUrl) {
                    text += element.toUrl().toDisplayString();
                    text += u'\n';
                    ++numUrls;
                }
            }
            if (numUrls == 1)
                text.chop(1); // no final '\n' if there's only one URL
            data = QVariant(text);
        }
    }

    if (data.metaType() == type || !data.isValid())
        return data;

    // provide more conversion possibilities than just what QVariant provides

    // URLs can be lists as well...
    if ((typeId == QMetaType::QUrl && data.metaType().id() == QMetaType::QVariantList)
        || (typeId == QMetaType::QVariantList && data.metaType().id() == QMetaType::QUrl))
        return data;

    // images and pixmaps are interchangeable
    if ((typeId == QMetaType::QPixmap && data.metaType().id() == QMetaType::QImage)
        || (typeId == QMetaType::QImage && data.metaType().id() == QMetaType::QPixmap))
        return data;

    if (data.metaType().id() == QMetaType::QByteArray) {
        // see if we can convert to the requested type
        switch (typeId) {
        case QMetaType::QString: {
            const QByteArray ba = data.toByteArray();
            if (ba.isNull())
                return QVariant();
            if (format == "text/html"_L1) {
                QStringDecoder decoder = QStringDecoder::decoderForHtml(ba);
                if (decoder.isValid()) {
                    return QString(decoder(ba));
                }
                // fall back to utf8
            }
            return QString::fromUtf8(ba);
        }
        case QMetaType::QColor: {
            QVariant newData = data;
            newData.convert(QMetaType(QMetaType::QColor));
            return newData;
        }
        case QMetaType::QVariantList: {
            if (format != "text/uri-list"_L1)
                break;
            Q_FALLTHROUGH();
        }
        case QMetaType::QUrl: {
            auto bav = data.view<QByteArrayView>();
            // Qt 3.x will send text/uri-list with a trailing
            // null-terminator (that is *not* sent for any other
            // text/* mime-type), so chop it off
            if (bav.endsWith('\0'))
                bav.chop(1);
            return dataToUrls(bav);
        }
        default:
            break;
        }

    } else if (typeId == QMetaType::QByteArray) {

        // try to convert to bytearray
        switch (data.metaType().id()) {
        case QMetaType::QByteArray:
        case QMetaType::QColor:
            return data.toByteArray();
        case QMetaType::QString:
            return data.toString().toUtf8();
        case QMetaType::QUrl:
            return data.toUrl().toEncoded();
        case QMetaType::QVariantList: {
            // has to be list of URLs
            QByteArray result;
            const QList<QVariant> list = data.toList();
            for (const auto &element : list) {
                if (element.metaType().id() == QMetaType::QUrl) {
                    result += element.toUrl().toEncoded();
                    result += "\r\n";
                }
            }
            if (!result.isEmpty())
                return result;
            break;
        }
        default:
            break;
        }
    }
    return data;
}

/*!
    \class QMimeData
    \inmodule QtCore
    \brief The QMimeData class provides a container for data that records information
    about its MIME type.

    QMimeData is used to describe information that can be stored in
    the \l{QClipboard}{clipboard}, and transferred via the \l{drag
    and drop} mechanism. QMimeData objects associate the data that
    they hold with the corresponding MIME types to ensure that
    information can be safely transferred between applications, and
    copied around within the same application.

    QMimeData objects are usually created using \c new and supplied
    to QDrag or QClipboard objects. This is to enable Qt to manage
    the memory that they use.

    A single QMimeData object can store the same data using several
    different formats at the same time. The formats() function
    returns a list of the available formats in order of preference.
    The data() function returns the raw data associated with a MIME
    type, and setData() allows you to set the data for a MIME type.

    For the most common MIME types, QMimeData provides convenience
    functions to access the data:

    \table
    \header \li Tester       \li Getter       \li Setter           \li MIME Types
    \row    \li hasText()    \li text()       \li setText()        \li \c text/plain
    \row    \li hasHtml()    \li html()       \li setHtml()        \li \c text/html
    \row    \li hasUrls()    \li urls()       \li setUrls()        \li \c text/uri-list
    \row    \li hasImage()   \li imageData()  \li setImageData()   \li \c image/ *
    \row    \li hasColor()   \li colorData()  \li setColorData()   \li \c application/x-color
    \endtable

    For example, if your write a widget that accepts URL drags, you
    would end up writing code like this:

    \snippet code/src_corelib_kernel_qmimedata.cpp 0

    There are three approaches for storing custom data in a QMimeData
    object:

    \list 1
    \li  Custom data can be stored directly in a QMimeData object as a
        QByteArray using setData(). For example:

        \snippet code/src_corelib_kernel_qmimedata.cpp 1

    \li  We can subclass QMimeData and reimplement hasFormat(),
        formats(), and retrieveData().

    \li  If the drag and drop operation occurs within a single
        application, we can subclass QMimeData and add extra data in
        it, and use a qobject_cast() in the receiver's drop event
        handler. For example:

        \snippet code/src_corelib_kernel_qmimedata.cpp 2
    \endlist

    \section1 Platform-Specific MIME Types

    On Windows, formats() will also return custom formats available
    in the MIME data, using the \c{x-qt-windows-mime} subtype to
    indicate that they represent data in non-standard formats.
    The formats will take the following form:

    \snippet code/src_corelib_kernel_qmimedata.cpp 3

    The following are examples of custom MIME types:

    \snippet code/src_corelib_kernel_qmimedata.cpp 4

    The \c value declaration of each format describes the way in which the
    data is encoded.

    In some cases (e.g. dropping multiple email attachments), multiple data
    values are available. They can be accessed by adding an \c index value:

    \snippet code/src_corelib_kernel_qmimedata.cpp 8

    On Windows, the MIME format does not always map directly to the
    clipboard formats. Qt provides QWindowsMimeConverter to map clipboard
    formats to open-standard MIME formats. Similarly, the
    QUtiMimeConverter maps MIME to Uniform Type Identifiers on macOS and iOS.

    \sa QClipboard, QDragEnterEvent, QDragMoveEvent, QDropEvent, QDrag,
        {Drag and Drop}
*/

/*!
    Constructs a new MIME data object with no data in it.
*/
QMimeData::QMimeData()
    : QObject(*new QMimeDataPrivate, nullptr)
{
}

/*!
    Destroys the MIME data object.
*/
QMimeData::~QMimeData()
{
}

/*!
    Returns a list of URLs contained within the MIME data object.

    URLs correspond to the MIME type \c text/uri-list.

    \sa hasUrls(), data()
*/
QList<QUrl> QMimeData::urls() const
{
    Q_D(const QMimeData);
    QVariant data = d->retrieveTypedData(textUriListLiteral(), QMetaType(QMetaType::QVariantList));
    QList<QUrl> urls;
    if (data.metaType().id() == QMetaType::QUrl)
        urls.append(data.toUrl());
    else if (data.metaType().id() == QMetaType::QVariantList) {
        const QList<QVariant> list = data.toList();
        for (const auto &element : list) {
            if (element.metaType().id() == QMetaType::QUrl)
                urls.append(element.toUrl());
        }
    }
    return urls;
}

/*!
    Sets the URLs stored in the MIME data object to those specified by \a urls.

    URLs correspond to the MIME type \c text/uri-list.

    Since Qt 5.0, setUrls also exports the urls as plain text, if setText
    was not called before, to make it possible to drop them into any lineedit
    and text editor.

    \sa hasUrls(), setData()
*/
void QMimeData::setUrls(const QList<QUrl> &urls)
{
    Q_D(QMimeData);
    d->setData(textUriListLiteral(), QList<QVariant>(urls.cbegin(), urls.cend()));
}

/*!
    Returns \c true if the object can return a list of urls; otherwise
    returns \c false.

    URLs correspond to the MIME type \c text/uri-list.

    \sa setUrls(), urls(), hasFormat()
*/
bool QMimeData::hasUrls() const
{
    return hasFormat(textUriListLiteral());
}


/*!
    Returns the plain text (MIME type \c text/plain) representation of
    the data if this object contains plain text. If it contains some other
    content, this function makes a best effort to convert it to plain text.

    \sa hasText(), html(), data()
*/
QString QMimeData::text() const
{
    Q_D(const QMimeData);
    QVariant utf8Text = d->retrieveTypedData(textPlainUtf8Literal(), QMetaType(QMetaType::QString));
    if (!utf8Text.isNull())
        return utf8Text.toString();

    QVariant data = d->retrieveTypedData(textPlainLiteral(), QMetaType(QMetaType::QString));
    return data.toString();
}

/*!
    Sets \a text as the plain text (MIME type \c text/plain) used to
    represent the data.

    \sa hasText(), setHtml(), setData()
*/
void QMimeData::setText(const QString &text)
{
    Q_D(QMimeData);
    d->setData(textPlainLiteral(), text);
}

/*!
    Returns \c true if the object can return plain text (MIME type \c
    text/plain); otherwise returns \c false.

    \sa setText(), text(), hasHtml(), hasFormat()
*/
bool QMimeData::hasText() const
{
    return hasFormat(textPlainLiteral()) || hasUrls();
}

/*!
    Returns a string if the data stored in the object is HTML (MIME
    type \c text/html); otherwise returns an empty string.

    \sa hasHtml(), setData()
*/
QString QMimeData::html() const
{
    Q_D(const QMimeData);
    QVariant data = d->retrieveTypedData(textHtmlLiteral(), QMetaType(QMetaType::QString));
    return data.toString();
}

/*!
    Sets \a html as the HTML (MIME type \c text/html) used to
    represent the data.

    \sa hasHtml(), setText(), setData()
*/
void QMimeData::setHtml(const QString &html)
{
    Q_D(QMimeData);
    d->setData(textHtmlLiteral(), html);
}

/*!
    Returns \c true if the object can return HTML (MIME type \c
    text/html); otherwise returns \c false.

    \sa setHtml(), html(), hasFormat()
*/
bool QMimeData::hasHtml() const
{
    return hasFormat(textHtmlLiteral());
}

/*!
    Returns a QVariant storing a QImage if the object can return an
    image; otherwise returns a null variant.

    A QVariant is used because QMimeData belongs to the Qt Core
    module, whereas QImage belongs to Qt GUI. To convert the
    QVariant to a QImage, simply use qvariant_cast(). For example:

    \snippet code/src_corelib_kernel_qmimedata.cpp 5

    \sa hasImage()
*/
QVariant QMimeData::imageData() const
{
    Q_D(const QMimeData);
    return d->retrieveTypedData(applicationXQtImageLiteral(), QMetaType(QMetaType::QImage));
}

/*!
    Sets the data in the object to the given \a image.

    A QVariant is used because QMimeData belongs to the Qt Core
    module, whereas QImage belongs to Qt GUI. The conversion
    from QImage to QVariant is implicit. For example:

    \snippet code/src_corelib_kernel_qmimedata.cpp 6

    \sa hasImage(), setData()
*/
void QMimeData::setImageData(const QVariant &image)
{
    Q_D(QMimeData);
    d->setData(applicationXQtImageLiteral(), image);
}

/*!
    Returns \c true if the object can return an image; otherwise returns
    false.

    \sa setImageData(), imageData(), hasFormat()
*/
bool QMimeData::hasImage() const
{
    return hasFormat(applicationXQtImageLiteral());
}

/*!
    Returns a color if the data stored in the object represents a
    color (MIME type \c application/x-color); otherwise returns a
    null variant.

    A QVariant is used because QMimeData belongs to the Qt Core
    module, whereas QColor belongs to Qt GUI. To convert the
    QVariant to a QColor, simply use qvariant_cast(). For example:

    \snippet code/src_corelib_kernel_qmimedata.cpp 7

    \sa hasColor(), setColorData(), data()
*/
QVariant QMimeData::colorData() const
{
    Q_D(const QMimeData);
    return d->retrieveTypedData(applicationXColorLiteral(), QMetaType(QMetaType::QColor));
}

/*!
    Sets the color data in the object to the given \a color.

    Colors correspond to the MIME type \c application/x-color.

    \sa hasColor(), setData()
*/
void QMimeData::setColorData(const QVariant &color)
{
    Q_D(QMimeData);
    d->setData(applicationXColorLiteral(), color);
}


/*!
    Returns \c true if the object can return a color (MIME type \c
    application/x-color); otherwise returns \c false.

    \sa setColorData(), colorData(), hasFormat()
*/
bool QMimeData::hasColor() const
{
    return hasFormat(applicationXColorLiteral());
}

/*!
    Returns the data stored in the object in the format described by
    the MIME type specified by \a mimeType. If this object does not contain
    data for the \a mimeType MIME type (see hasFormat()), this function may
    perform a best effort conversion to it.

    \sa hasFormat(), setData()
*/
QByteArray QMimeData::data(const QString &mimeType) const
{
    Q_D(const QMimeData);
    QVariant data = d->retrieveTypedData(mimeType, QMetaType(QMetaType::QByteArray));
    return data.toByteArray();
}

/*!
    Sets the data associated with the MIME type given by \a mimeType
    to the specified \a data.

    For the most common types of data, you can call the higher-level
    functions setText(), setHtml(), setUrls(), setImageData(), and
    setColorData() instead.

    Note that if you want to use a custom data type in an item view drag and drop
    operation, you must register it as a Qt \l{QMetaType}{meta type}, using the
    Q_DECLARE_METATYPE() macro, and implement stream operators for it.

    \sa hasFormat(), QMetaType, {QMetaType::}{Q_DECLARE_METATYPE()}
*/
void QMimeData::setData(const QString &mimeType, const QByteArray &data)
{
    Q_D(QMimeData);

    if (mimeType == "text/uri-list"_L1) {
        auto ba = QByteArrayView(data);
        if (ba.endsWith('\0'))
            ba.chop(1);
        d->setData(mimeType, dataToUrls(ba));
    } else {
        d->setData(mimeType, QVariant(data));
    }
}

/*!
    Returns \c true if the object can return data for the MIME type
    specified by \a mimeType; otherwise returns \c false.

    For the most common types of data, you can call the higher-level
    functions hasText(), hasHtml(), hasUrls(), hasImage(), and
    hasColor() instead.

    \sa formats(), setData(), data()
*/
bool QMimeData::hasFormat(const QString &mimeType) const
{
    // formats() is virtual and could be reimplemented in sub-classes,
    // so we have to use it here.
    return formats().contains(mimeType);
}

/*!
    Returns a list of formats supported by the object. This is a list
    of MIME types for which the object can return suitable data. The
    formats in the list are in a priority order.

    For the most common types of data, you can call the higher-level
    functions hasText(), hasHtml(), hasUrls(), hasImage(), and
    hasColor() instead.

    \sa hasFormat(), setData(), data()
*/
QStringList QMimeData::formats() const
{
    Q_D(const QMimeData);
    QStringList list;
    list.reserve(static_cast<int>(d->dataList.size()));
    for (auto &e : d->dataList)
        list += e.format;
    return list;
}

/*!
    Returns a variant with the given \a type containing data for the
    MIME type specified by \a mimeType. If the object does not
    support the MIME type or variant type given, a null variant is
    returned instead.

    This function is called by the general data() getter and by the
    convenience getters (text(), html(), urls(), imageData(), and
    colorData()). You can reimplement it if you want to store your
    data using a custom data structure (instead of a QByteArray,
    which is what setData() provides). You would then also need
    to reimplement hasFormat() and formats().

    \sa data()
*/
QVariant QMimeData::retrieveData(const QString &mimeType, QMetaType type) const
{
    Q_UNUSED(type);
    Q_D(const QMimeData);
    return d->getData(mimeType);
}

/*!
    Removes all the MIME type and data entries in the object.
*/
void QMimeData::clear()
{
    Q_D(QMimeData);
    d->dataList.clear();
}

/*!
    \since 4.4

    Removes the data entry for \a mimeType in the object.
*/
void QMimeData::removeFormat(const QString &mimeType)
{
    Q_D(QMimeData);
    d->removeData(mimeType);
}

QT_END_NAMESPACE

#include "moc_qmimedata.cpp"
