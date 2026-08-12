// SPDX-License-Identifier: GPL-2.0
// AI-generated (Claude)
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QJsonObject>
#include <QMainWindow>
#include <QThread>
#include <QVector>

class QCheckBox;
class QDialog;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QRadioButton;
class QStackedWidget;
class QTableWidget;
class QWidget;
class SuuntoWorker;

class MainWindow : public QMainWindow {
	Q_OBJECT
public:
	explicit MainWindow(QWidget *parent = nullptr);
	~MainWindow() override;

private slots:
	void onOpenLoginDialog();
	void onDialogSignInClicked();
	void onLoginSucceeded(const QString &email, bool usedCachedSession);
	void onLoginFailed(const QString &error);
	void onDiveListReady(const QVector<QJsonObject> &dives);
	void onDiveListFailed(const QString &error);
	void onBrowseOutDir();
	void onBrowseLogFile();
	void onBrowseLogFolder();
	void onModeChanged();
	void onSelectAllToggled(bool checked);
	void onExportClicked();
	void onExportProgress(int current, int total, const QString &message);
	void onExportFailed(int index, const QString &error);
	void onExportFinished(int succeeded, int total);
	void onDirectImportFinished(int importedCount, const QString &error);

private:
	QWidget *buildLoginPage();
	QWidget *buildMainPage();
	QDialog *buildLoginDialog();
	void populateDiveTable();
	void setLoginPageBusy(bool busy);
	void setLoginDialogBusy(bool busy);

	QThread workerThread_;
	SuuntoWorker *worker_ = nullptr;
	QString sessionPath_;
	bool firstLoginAttempt_ = true;

	QVector<QJsonObject> dives_;

	QStackedWidget *stackedWidget_ = nullptr;
	QWidget *loginPage_ = nullptr;
	QWidget *mainPage_ = nullptr;

	// loginPage_: shown when there's no usable cached session. Its only
	// job is to offer a "Sign in" button that pops up loginDialog_ --
	// email/password are entered there, never on a page that's part of
	// the main window layout.
	QPushButton *signInButton_ = nullptr;
	QLabel *loginStatusLabel_ = nullptr;

	// loginDialog_: email/password are used only to obtain a session
	// token (see SuuntoClient::login()/saveSession()) and are never
	// written to disk -- only the resulting session key is cached.
	QDialog *loginDialog_ = nullptr;
	QLineEdit *emailEdit_ = nullptr;
	QLineEdit *passwordEdit_ = nullptr;
	QPushButton *dialogSignInButton_ = nullptr;
	QLabel *dialogStatusLabel_ = nullptr;

	QTableWidget *diveTable_ = nullptr;
	QCheckBox *selectAllCheck_ = nullptr;

	QRadioButton *exportFilesMode_ = nullptr;
	QRadioButton *directImportMode_ = nullptr;

	QWidget *exportFilesRow_ = nullptr;
	QLineEdit *outDirEdit_ = nullptr;
	QCheckBox *alsoFitCheck_ = nullptr;

	QWidget *directImportRow_ = nullptr;
	QLineEdit *logPathEdit_ = nullptr;

	QPushButton *exportButton_ = nullptr;
	QProgressBar *progressBar_ = nullptr;
	QPlainTextEdit *logView_ = nullptr;
};

#endif
