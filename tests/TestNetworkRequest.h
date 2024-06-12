
#ifndef KEEPASSXC_TESTNETWORKREQUEST_HPP
#define KEEPASSXC_TESTNETWORKREQUEST_HPP

#include <QObject>

class TestNetworkRequest : public QObject
{
    Q_OBJECT

private slots:
    void testNetworkRequest();
    void testNetworkRequest_data();

    void testNetworkRequestTimeout();
    void testNetworkRequestTimeout_data();

    void testNetworkRequestRedirects();
    void testNetworkRequestRedirects_data();

    void testNetworkRequestTimeoutWithRedirects();

    void testNetworkRequestSecurityParameter();
    void testNetworkRequestSecurityParameter_data();
};

#endif // KEEPASSXC_TESTNETWORKREQUEST_HPP
