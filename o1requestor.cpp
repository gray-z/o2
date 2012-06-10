#include <QDebug>
#include <QTimer>
#include <QDateTime>
#include <QNetworkReply>
#include <QNetworkAccessManager>

#include "o1requestor.h"

/// A timer connected to a network reply.
class TimedReply: public QTimer {
    Q_OBJECT

public:
    explicit TimedReply(QNetworkReply *parent): QTimer(parent) {
        setSingleShot(true);
        setInterval(60 * 1000); // FIXME: Expose me
        connect(this, SIGNAL(error(QNetworkReply::NetworkError)), parent, SIGNAL(error(QNetworkReply::NetworkError)));
        connect(this, SIGNAL(timeout()), this, SLOT(onTimeout()));
    }

signals:
    void error(QNetworkReply::NetworkError);

public slots:
    void onTimeout() {emit error(QNetworkReply::TimeoutError);}
};

O1Requestor::O1Requestor(QNetworkAccessManager *manager, O1 *authenticator, QObject *parent): QObject(parent) {
    manager_ = manager;
    authenticator_ = authenticator;
    connect(authenticator, SIGNAL(refreshFinished(QNetworkReply::NetworkError)), this, SLOT(onRefreshFinished(QNetworkReply::NetworkError)), Qt::QueuedConnection);
}

QNetworkReply *O1Requestor::get(const QNetworkRequest &req, const QList<O1RequestParameter> &signingParameters) {
    QNetworkRequest request = setup(req, signingParameters, QNetworkAccessManager::GetOperation);
    return addTimer(manager_->get(request));
}

QNetworkReply *O1Requestor::post(const QNetworkRequest &req, const QList<O1RequestParameter> &signingParameters, const QByteArray &data) {
    QNetworkRequest request = setup(req, signingParameters, QNetworkAccessManager::PostOperation);
    return addTimer(manager_->post(request, data));
}

QNetworkReply *O1Requestor::put(const QNetworkRequest &req, const QList<O1RequestParameter> &signingParameters, const QByteArray &data) {
    QNetworkRequest request = setup(req, signingParameters, QNetworkAccessManager::PutOperation);
    return addTimer(manager_->put(request, data));
}

QNetworkReply *O1Requestor::addTimer(QNetworkReply *reply) {
    (void)new TimedReply(reply);
    return reply;
}

QNetworkRequest O1Requestor::setup(const QNetworkRequest &req, const QList<O1RequestParameter> &signingParameters, QNetworkAccessManager::Operation operation) {
    QNetworkRequest request(req);
    QList<O1RequestParameter> oauthParams;
    oauthParams.append(O1RequestParameter("oauth_consumer_key", authenticator_->clientId().toAscii()));
    oauthParams.append(O1RequestParameter("oauth_nonce", QString::number(qrand()).toAscii()));
    oauthParams.append(O1RequestParameter("oauth_signature_method", "HMAC-SHA1"));
    oauthParams.append(O1RequestParameter("oauth_timestamp", QString::number(QDateTime::currentDateTimeUtc().toTime_t()).toAscii()));
    oauthParams.append(O1RequestParameter("oauth_version", "1.0"));
    oauthParams.append(O1RequestParameter("oauth_token", authenticator_->token().toAscii()));

    QByteArray signature = authenticator_->sign(oauthParams, signingParameters, req.url(), operation, authenticator_->clientSecret(), authenticator_->tokenSecret());
    oauthParams.append(O1RequestParameter("oauth_signature", signature));

    request.setRawHeader("Authorization", O1::buildAuthorizationHeader(oauthParams));
    return request;
}

#include "o1requestor.moc"
