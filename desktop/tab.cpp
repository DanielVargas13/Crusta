#include "bookmarks.h"
#include "browser.h"
#include "history.h"
#include "search_engine.h"
#include "tab.h"
#include "webview.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QIcon>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWebEngineHistory>
#include <QWebEngineProfile>
#include <QWebEngineSettings>

Tab::Tab(QWidget *parent)
    : QWidget(parent)
{
}

void WebTab::setup_toolbar()
{
    auto create_tool_button = [](const QString &name) {
        QToolButton *button = new QToolButton;
        button->setAutoRaise(true);
        button->setIcon(QIcon::fromTheme(name));
        return button;
    };

    m_back_button = create_tool_button(QStringLiteral("go-previous"));
    m_forward_button = create_tool_button(QStringLiteral("go-next"));
    m_refresh_button = create_tool_button(QStringLiteral("view-refresh"));
    m_home_button = create_tool_button(QStringLiteral("go-home"));
    m_download_button = create_tool_button(QStringLiteral("edit-download"));

    m_address_bar = new QLineEdit;
    m_bookmark_action = m_address_bar->addAction(QIcon::fromTheme(QStringLiteral("bookmark-new")), QLineEdit::TrailingPosition);

    m_toolbar->addWidget(m_back_button);
    m_toolbar->addWidget(m_forward_button);
    m_toolbar->addWidget(m_refresh_button);
    m_toolbar->addWidget(m_home_button);
    m_toolbar->addWidget(m_address_bar);
    m_toolbar->addWidget(m_download_button);

    connect(m_back_button, &QToolButton::clicked, m_webview, &WebView::back);
    connect(m_forward_button, &QToolButton::clicked, m_webview, &WebView::forward);
    connect(m_refresh_button, &QToolButton::clicked, [this] {
        const QString icon_name = m_refresh_button->icon().name();
        if (icon_name == QStringLiteral("view-refresh")) {
            m_webview->reload();
        } else {
            m_webview->stop();
        }
    });
    connect(m_home_button, &QToolButton::clicked, m_webview, &WebView::home);
    connect(m_address_bar, &QLineEdit::returnPressed, [this] {
        const QString text = m_address_bar->text();
        const QUrl url = QUrl::fromUserInput(text);
        if (url.isValid()) {
            if (url.scheme() == QStringLiteral("javascript")) {
                const QString code = url.toString((QUrl::RemoveScheme | QUrl::FullyDecoded) & ~(QUrl::EncodeSpaces));
                m_webview->page()->runJavaScript(code);
                return ;
            } else if (url.host() == QStringLiteral("localhost") || url.host().split(QStringLiteral(".")).count() > 1) {
                m_webview->load(url);
                return ;
            }
        }

        SearchEngine engine = browser->search_model()->default_engine();
        m_webview->load(engine.query_url.replace(QStringLiteral("{searchTerms}"), text));
        m_webview->setFocus();
    });

    connect(m_bookmark_action, &QAction::triggered, this, &WebTab::bookmark);

    connect(m_download_button, &QToolButton::clicked, [this] {
        QWidget *widget = (QWidget *)browser->download_widget();
        widget->setWindowFlag(Qt::Popup);
        widget->show();
        QPoint pos = m_download_button->rect().bottomRight() - QPoint(widget->rect().width(), 0);
        widget->move(m_download_button->mapToGlobal(pos));
    });

    connect(m_webview, &WebView::urlChanged, [this] (const QUrl &address) {
        m_address_bar->setText(address.toEncoded());
        m_address_bar->setCursorPosition(0);
    });
    connect(m_webview, &WebView::loadStarted, [this] {
        m_refresh_button->setIcon(QIcon::fromTheme(QStringLiteral("process-stop")));
    });
    connect(m_webview, &WebView::loadFinished, [this] {
        m_back_button->setEnabled(m_webview->history()->canGoBack());
        m_forward_button->setEnabled(m_webview->history()->canGoForward());
        m_refresh_button->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));
    });
}

WebTab::WebTab(QWidget *parent)
    : Tab(parent)
{
    m_toolbar = new QToolBar;
    m_webview = new WebView;

    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);
    setLayout(vbox);

    vbox->addWidget(m_toolbar);
    vbox->addWidget(m_webview);

    setup_toolbar();

    connect(m_webview, &WebView::titleChanged, [this] (const QString &title) { emit title_changed(title); });
    connect(m_webview, &WebView::iconChanged, [this] (const QIcon &icon) { emit icon_changed(icon); });
}

QToolBar *WebTab::toolbar() const
{
    return m_toolbar;
}

QLineEdit *WebTab::address_bar() const
{
    return m_address_bar;
}

WebView *WebTab::webview() const
{
    return m_webview;
}

void WebTab::bookmark()
{
    BookmarkTreeNode *node = new BookmarkTreeNode(BookmarkTreeNode::Address);
    node->title = m_webview->title();
    node->address = m_webview->url().toString();
    browser->bookmark_model()->add_bookmark(nullptr, node);
}

void ManagerTab::setup_toolbar()
{
    m_toolbar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    QAction *settings = m_toolbar->addAction(QIcon::fromTheme(QStringLiteral("configure")), QStringLiteral("Settings"));
    QAction *history = m_toolbar->addAction(QIcon::fromTheme(QStringLiteral("appointment-new")), QStringLiteral("History"));
    QAction *bookmarks = m_toolbar->addAction(QIcon::fromTheme(QStringLiteral("bookmark-new")), QStringLiteral("Bookmarks"));
    QAction *search = m_toolbar->addAction(QIcon::fromTheme(QStringLiteral("edit-find")), QStringLiteral("Search"));

    connect(settings, &QAction::triggered, this, &ManagerTab::open_settings);
    connect(history, &QAction::triggered, this, &ManagerTab::open_history);
    connect(bookmarks, &QAction::triggered, this, &ManagerTab::open_bookmarks);
    connect(search, &QAction::triggered, this, &ManagerTab::open_search);
}

void ManagerTab::setup_stacked_widget()
{
    HistoryWidget *history_widget = new HistoryWidget;
    m_stacked_widget->addWidget(history_widget);

    BookmarkWidget *bookmark_widget = new BookmarkWidget;
    m_stacked_widget->addWidget(bookmark_widget);

    SearchWidget *search_widget = new SearchWidget;
    m_stacked_widget->addWidget(search_widget);
}

void ManagerTab::setup_settings_widget()
{
    QScrollArea *scroll_area = new QScrollArea;
    QWidget *widget = new QWidget;
    QVBoxLayout *vbox = new QVBoxLayout;
    widget->setLayout(vbox);

    QGroupBox *browsing_group = new QGroupBox;
    browsing_group->setTitle(QStringLiteral("Browsing"));
    vbox->addWidget(browsing_group);
    {
        QVBoxLayout *vbox = new QVBoxLayout;
        browsing_group->setLayout(vbox);

        QGridLayout *grid = new QGridLayout;
        vbox->addLayout(grid);

        QLineEdit *homepage = new QLineEdit;
        homepage->setText(m_settings.value(QStringLiteral("browsing/homepage"), QStringLiteral("browser:startpage")).toString());
        connect(homepage, &QLineEdit::textChanged, [this] (const QString &text) {
            m_settings.setValue(QStringLiteral("browsing/homepage"), text);
        });

        grid->addWidget(new QLabel(QStringLiteral("Homepage")), 0, 0);
        grid->addWidget(homepage, 0, 1);
    }

    QGroupBox *download_group = new QGroupBox;
    download_group->setTitle(QStringLiteral("Downloads"));
    vbox->addWidget(download_group);
    {
        QVBoxLayout *vbox = new QVBoxLayout;
        download_group->setLayout(vbox);

        QGridLayout *grid = new QGridLayout;
        vbox->addLayout(grid);

        QLineEdit *download_path = new QLineEdit;
        download_path->setText(browser->web_profile()->downloadPath());
        connect(download_path, &QLineEdit::textChanged, [this] (const QString &text) {
            m_settings.setValue(QStringLiteral("downloads/path"), text);
            browser->web_profile()->setDownloadPath(text);
        });

        grid->addWidget(new QLabel(QStringLiteral("Download Path")), 0, 0);
        grid->addWidget(download_path, 0, 1);

        QCheckBox *ask_for_downloads = new QCheckBox(QStringLiteral("Always ask before downloading"));
        ask_for_downloads->setChecked(m_settings.value(QStringLiteral("downloads/ask"), true).toBool());
        connect(ask_for_downloads, &QCheckBox::clicked, [this](bool checked) {
            m_settings.setValue(QStringLiteral("downloads/ask"), checked);
        });
        vbox->addWidget(ask_for_downloads);
    }

    QGroupBox *privacy_group = new QGroupBox;
    privacy_group->setTitle(QStringLiteral("Privacy"));
    vbox->addWidget(privacy_group);
    {
        QVBoxLayout *vbox = new QVBoxLayout;
        privacy_group->setLayout(vbox);

        QHBoxLayout *hbox0 = new QHBoxLayout;
        vbox->addLayout(hbox0);
        hbox0->addWidget(new QLabel(QStringLiteral("User Agent")));
        QLineEdit *user_agent = new QLineEdit;
        user_agent->setText(browser->web_profile()->httpUserAgent());
        connect(user_agent, &QLineEdit::textChanged, [this] (const QString &text) {
            m_settings.setValue(QStringLiteral("privacy/user_agent"), text);
            browser->web_profile()->setHttpUserAgent(text);
        });
        hbox0->addWidget(user_agent);

        QCheckBox *dnt = new QCheckBox(QStringLiteral("Send Do Not Track header"));
        dnt->setChecked(m_settings.value(QStringLiteral("privacy/dnt"), true).toBool());
        connect(dnt, &QCheckBox::clicked, [this] (bool checked) {
            m_settings.setValue(QStringLiteral("privacy/dnt"), checked);
        });
        vbox->addWidget(dnt);

        QCheckBox *allow_third_party_cookies = new QCheckBox(QStringLiteral("Allow third party cookies*"));
        allow_third_party_cookies->setChecked(m_settings.value(QStringLiteral("privacy/allow_third_party_cookies"), false).toBool());
        connect(allow_third_party_cookies, &QCheckBox::clicked, [this](bool checked) {
            m_settings.setValue(QStringLiteral("privacy/allow_third_party_cookies"), checked);
        });
        vbox->addWidget(allow_third_party_cookies);

        QCheckBox *block_all_cookies = new QCheckBox(QStringLiteral("Block all cookies*"));
        block_all_cookies->setChecked(m_settings.value(QStringLiteral("privacy/block_all_cookies"), false).toBool());
        connect(block_all_cookies, &QCheckBox::clicked, [this] (bool checked) {
            m_settings.setValue(QStringLiteral("privacy/block_all_cookies"), checked);
        });
        vbox->addWidget(block_all_cookies);
    }

    QGroupBox *websettings_group = new QGroupBox;
    websettings_group->setTitle(QStringLiteral("Web Engine"));
    vbox->addWidget(websettings_group);
    {
        QVBoxLayout *vbox = new QVBoxLayout;
        websettings_group->setLayout(vbox);

#define ADD_CHECKBOX(name, description, attribute) \
    QCheckBox *name = new QCheckBox(QStringLiteral(description)); \
    name->setChecked(browser->web_profile()->settings()->testAttribute(attribute)); \
    vbox->addWidget(name); \
    connect(name, &QCheckBox::clicked, [this, name] { \
        browser->web_profile()->settings()->setAttribute(attribute, name->isChecked()); \
        m_settings.setValue("websettings/" #name, name->isChecked()); \
    }); \

        ADD_CHECKBOX(auto_load_images, "Auto load images", QWebEngineSettings::AutoLoadImages)
        ADD_CHECKBOX(javascript_enabled, "JavaScript enabled", QWebEngineSettings::JavascriptEnabled)
        ADD_CHECKBOX(javascript_can_open_windows, "JavaScript can open windows", QWebEngineSettings::JavascriptCanOpenWindows)
        ADD_CHECKBOX(javascript_can_access_clipboard, "JavaScript can access clipboard", QWebEngineSettings::JavascriptCanAccessClipboard)
        ADD_CHECKBOX(links_included_in_focus_chain, "Links included in focus chain", QWebEngineSettings::LinksIncludedInFocusChain)
        ADD_CHECKBOX(local_storage_enabled, "Local storage enabled", QWebEngineSettings::LocalStorageEnabled)
        ADD_CHECKBOX(local_content_can_access_remote_urls, "Local content can access remote urls", QWebEngineSettings::LocalContentCanAccessRemoteUrls)
        ADD_CHECKBOX(xss_auditing_enabled, "XSS auditing enabled", QWebEngineSettings::XSSAuditingEnabled)
        ADD_CHECKBOX(spatial_navigation_enabled, "Spatial navigation enabled", QWebEngineSettings::SpatialNavigationEnabled)
        ADD_CHECKBOX(local_content_can_access_file_urls, "Local content can access file urls", QWebEngineSettings::LocalContentCanAccessFileUrls)
        ADD_CHECKBOX(hyperlink_auditing_enabled, "Hyperlink auditing enabled", QWebEngineSettings::HyperlinkAuditingEnabled)
        ADD_CHECKBOX(scroll_animator_enabled, "Scroll animator enabled", QWebEngineSettings::ScrollAnimatorEnabled)
        ADD_CHECKBOX(error_page_enabled, "Error page enabled", QWebEngineSettings::ErrorPageEnabled);
        ADD_CHECKBOX(plugins_enabled, "Plugins enabled", QWebEngineSettings::PluginsEnabled)
        ADD_CHECKBOX(fullscreen_support_enabled, "Fullscreen support enabled", QWebEngineSettings::FullScreenSupportEnabled);
        ADD_CHECKBOX(screen_capture_enabled, "Screen capture enabled", QWebEngineSettings::ScreenCaptureEnabled);
        ADD_CHECKBOX(webgl_enabled, "WebGL enabled", QWebEngineSettings::WebGLEnabled);
        ADD_CHECKBOX(accelerated_2d_canvas_enabled, "Accelerated 2d canvas enabled", QWebEngineSettings::Accelerated2dCanvasEnabled)
        ADD_CHECKBOX(auto_load_icons_for_page, "Auto load icons for page", QWebEngineSettings::AutoLoadIconsForPage)
        ADD_CHECKBOX(touch_icons_enabled, "Touch icons enabled", QWebEngineSettings::TouchIconsEnabled)
        ADD_CHECKBOX(focus_on_navigation_enabled, "Focus on navigation enabled", QWebEngineSettings::FocusOnNavigationEnabled)
        ADD_CHECKBOX(print_element_backgrounds, "Print element backgrounds", QWebEngineSettings::PrintElementBackgrounds)
        ADD_CHECKBOX(allow_running_insecure_content, "Allow running insecure content", QWebEngineSettings::AllowRunningInsecureContent)
        ADD_CHECKBOX(allow_geolocation_on_insecure_origin, "Allow geolocation on insecure origins", QWebEngineSettings::AllowGeolocationOnInsecureOrigins)
        ADD_CHECKBOX(allow_window_activation_from_javascript, "Allow window activation from JavaScript", QWebEngineSettings::AllowWindowActivationFromJavaScript)
        ADD_CHECKBOX(show_scroll_bars, "Show scroll bars", QWebEngineSettings::ShowScrollBars)
        ADD_CHECKBOX(playback_requires_user_gesture, "Playback requires user gesture", QWebEngineSettings::PlaybackRequiresUserGesture)
        ADD_CHECKBOX(javascript_can_paste, "JavaScript can paste", QWebEngineSettings::JavascriptCanPaste)
        ADD_CHECKBOX(webrtc_public_interfaces_only, "WebRTC public interfaces only", QWebEngineSettings::WebRTCPublicInterfacesOnly)
        ADD_CHECKBOX(dns_prefetch_enabled, "DNS prefetch enabled", QWebEngineSettings::DnsPrefetchEnabled)
        ADD_CHECKBOX(pdf_viewer_enabled, "PDF viewer enabled", QWebEngineSettings::PdfViewerEnabled)

#undef ADD_CHECKBOX
    }

    QLabel *what_is_star = new QLabel(QStringLiteral("* Restart required"));
    vbox->addWidget(what_is_star);

    scroll_area->setWidget(widget);
    scroll_area->setWidgetResizable(true);
    m_stacked_widget->addWidget(scroll_area);
}

ManagerTab::ManagerTab(QWidget *parent)
    : Tab(parent)
{
    m_toolbar = new QToolBar;
    m_stacked_widget = new QStackedWidget;

    QVBoxLayout *vbox = new QVBoxLayout;
    setLayout(vbox);

    vbox->addWidget(m_toolbar);
    vbox->addWidget(m_stacked_widget);

    setup_toolbar();
    setup_settings_widget();
    setup_stacked_widget();
}

void ManagerTab::open_settings()
{
    m_stacked_widget->setCurrentIndex(0);
    emit title_changed(QStringLiteral("Settings"));
    emit icon_changed(QIcon::fromTheme(QStringLiteral("configure")));
}

void ManagerTab::open_history()
{
    m_stacked_widget->setCurrentIndex(1);
    emit title_changed(QStringLiteral("History"));
    emit icon_changed(QIcon::fromTheme(QStringLiteral("appointment-new")));
}

void ManagerTab::open_bookmarks()
{
    m_stacked_widget->setCurrentIndex(2);
    emit title_changed(QStringLiteral("Bookmarks"));
    emit icon_changed(QIcon::fromTheme(QStringLiteral("bookmark-new")));
}

void ManagerTab::open_search()
{
    m_stacked_widget->setCurrentIndex(3);
    emit title_changed(QStringLiteral("Search"));
    emit icon_changed(QIcon::fromTheme(QStringLiteral("edit-find")));
}
