#pragma once

#include <memory>

#include <QtCore/QJsonObject>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtCore/QVariantMap>
#include <QtMultimedia/QVideoSink>
#include <QtQuick/QQuickItem>

class QGCVideoFrameContextStore;
class QSGTexture;

/// Owns decoded pixels and provenance through Qt's frameSwapped presentation boundary.
/// This establishes application presentation, not physical scanout or undetectable device failure.
class PixEagleVideoItem : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVideoSink* videoSink READ videoSink NOTIFY videoSinkChanged)
    Q_PROPERTY(bool presentationKnown READ presentationKnown NOTIFY presentationChanged)
    Q_PROPERTY(bool frameFresh READ frameFresh NOTIFY presentationChanged)
    Q_PROPERTY(QSize sourceSize READ sourceSize NOTIFY presentationChanged)
    Q_PROPERTY(QVariantMap displayedContext READ displayedContext NOTIFY presentationChanged)
    Q_PROPERTY(bool selectionReady READ selectionReady NOTIFY selectionChanged)
    Q_PROPERTY(bool selectionActive READ selectionActive NOTIFY selectionChanged)
    Q_PROPERTY(QString selectionError READ selectionError NOTIFY selectionChanged)

public:
    explicit PixEagleVideoItem(QQuickItem* parent = nullptr);
    ~PixEagleVideoItem() override;

    QVideoSink* videoSink() const { return _videoSink; }

    bool presentationKnown() const;
    bool frameFresh() const;
    QSize sourceSize() const;
    QVariantMap displayedContext() const;
    bool selectionReady() const;
    bool selectionActive() const;
    QString selectionError() const;

    void setStream(std::shared_ptr<QGCVideoFrameContextStore> store, const QJsonObject& expectedContext);
    void clearStream();
    /// Reads the guarded frameSwapped record directly, independent of queued QML notifications.
    /// Stale/cached frames retain their identity with frame_fresh=false; unknown returns an empty map.
    Q_INVOKABLE QVariantMap capturePresentedContext() const;
    /// GUI-thread selection snapshot; coordinates belong to inputItem in logical pixels.
    /// Completion returns the original provenance/geometry and normalized encoded-image coordinates.
    QString beginSelection(QQuickItem* inputItem, const QPointF& start);
    QVariantMap finishSelection(const QString& token, const QPointF& end, bool rectangle);
    void cancelSelection();

signals:
    void videoSinkChanged();
    void presentationChanged();
    void frameContextRejected();
    void selectionChanged();
    void selectionInvalidated();

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;
    void releaseResources() override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void itemChange(ItemChange change, const ItemChangeData& value) override;
    virtual QSGTexture* createFrameTexture(QQuickWindow* window, const QImage& image);

private:
    struct State;
    void _replaceSink();
    void _consumeFrame(quint64 streamGeneration);
    void _attachWindow(QQuickWindow* window);
    void _observeAncestors();
    bool _surfaceUsable() const;
    void _refreshSurface();
    void _invalidateSurface();
    void _publishProperties();

    std::shared_ptr<State> _state;
    QVideoSink* _videoSink = nullptr;
    QPointer<QQuickWindow> _attachedWindow;
    QList<QMetaObject::Connection> _windowConnections;
    QList<QMetaObject::Connection> _ancestorConnections;
    QTimer _freshnessTimer;
    QVariantMap _lastContext;
    QSize _lastSize;
    quint64 _lastSelectionInvalidation = 0;
    bool _lastSelectionReady = false;
    bool _lastSelectionActive = false;
    QString _lastSelectionError;
};
