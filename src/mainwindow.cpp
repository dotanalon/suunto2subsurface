// SPDX-License-Identifier: GPL-2.0
// AI-generated (Claude)
#include "mainwindow.h"
#include "suuntoworker.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMetaObject>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

// Column 0's checkbox item carries the dive's original index into
// MainWindow::dives_/SuuntoWorker::dives_ in DiveIndexRole, since sorting
// reorders visual rows but must not change which dive a checkbox refers to.
constexpr int SortKeyRole = Qt::UserRole;
constexpr int DiveIndexRole = Qt::UserRole + 1;

// QTableWidgetItem::operator< (used when sorting is enabled) compares
// display text by default, which sorts dates/durations/depths as strings
// ("12:00" before "9:30"). Sort by the numeric value stashed in SortKeyRole
// instead, keeping the formatted text for display.
class NumericTableWidgetItem : public QTableWidgetItem {
public:
	using QTableWidgetItem::QTableWidgetItem;
	bool operator<(const QTableWidgetItem &other) const override
	{
		return data(SortKeyRole).toDouble() < other.data(SortKeyRole).toDouble();
	}
};

QJsonObject diveHeaderExtension(const QJsonObject &dive)
{
	for (const QJsonValue &v : dive["extensions"].toArray()) {
		QJsonObject e = v.toObject();
		if (e["type"].toString() == "DiveHeaderExtension")
			return e;
	}
	return {};
}

QString formatDuration(double seconds)
{
	int total = static_cast<int>(seconds + 0.5);
	return QString("%1:%2").arg(total / 60).arg(total % 60, 2, 10, QChar('0'));
}

}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
	setWindowTitle("suunto2subsurface");

	stackedWidget_ = new QStackedWidget(this);
	loginPage_ = buildLoginPage();
	mainPage_ = buildMainPage();
	loginDialog_ = buildLoginDialog();
	stackedWidget_->addWidget(loginPage_);
	stackedWidget_->addWidget(mainPage_);
	setCentralWidget(stackedWidget_);
	resize(700, 500);

	qRegisterMetaType<QVector<int>>("QVector<int>");
	qRegisterMetaType<QVector<QJsonObject>>("QVector<QJsonObject>");

	worker_ = new SuuntoWorker;
	worker_->moveToThread(&workerThread_);
	connect(&workerThread_, &QThread::finished, worker_, &QObject::deleteLater);
	connect(worker_, &SuuntoWorker::loginSucceeded, this, &MainWindow::onLoginSucceeded);
	connect(worker_, &SuuntoWorker::loginFailed, this, &MainWindow::onLoginFailed);
	connect(worker_, &SuuntoWorker::diveListReady, this, &MainWindow::onDiveListReady);
	connect(worker_, &SuuntoWorker::diveListFailed, this, &MainWindow::onDiveListFailed);
	connect(worker_, &SuuntoWorker::exportProgress, this, &MainWindow::onExportProgress);
	connect(worker_, &SuuntoWorker::exportFailed, this, &MainWindow::onExportFailed);
	connect(worker_, &SuuntoWorker::exportFinished, this, &MainWindow::onExportFinished);
	connect(worker_, &SuuntoWorker::directImportFinished, this, &MainWindow::onDirectImportFinished);
	workerThread_.start();

	sessionPath_ = SuuntoClient::defaultSessionPath();
	loginStatusLabel_->setText("Checking for a saved session...");
	setLoginPageBusy(true);
	QMetaObject::invokeMethod(worker_, "login", Qt::QueuedConnection, Q_ARG(QString, sessionPath_),
				   Q_ARG(QString, QString()), Q_ARG(QString, QString()));
}

MainWindow::~MainWindow()
{
	workerThread_.quit();
	workerThread_.wait();
}

QWidget *MainWindow::buildLoginPage()
{
	QWidget *page = new QWidget;
	QVBoxLayout *outer = new QVBoxLayout(page);
	outer->addStretch();

	QLabel *title = new QLabel("Sign in to your Suunto account");
	title->setAlignment(Qt::AlignHCenter);
	outer->addWidget(title);

	signInButton_ = new QPushButton("Sign in");
	connect(signInButton_, &QPushButton::clicked, this, &MainWindow::onOpenLoginDialog);
	QHBoxLayout *buttonRow = new QHBoxLayout;
	buttonRow->addStretch();
	buttonRow->addWidget(signInButton_);
	buttonRow->addStretch();
	outer->addLayout(buttonRow);

	loginStatusLabel_ = new QLabel;
	loginStatusLabel_->setAlignment(Qt::AlignHCenter);
	loginStatusLabel_->setWordWrap(true);
	outer->addWidget(loginStatusLabel_);

	outer->addStretch();
	return page;
}

QDialog *MainWindow::buildLoginDialog()
{
	// Email/password are only ever held here, in memory, for the duration
	// of a login attempt -- SuuntoClient::login() uses them to obtain a
	// session key, and only that session key (via SuuntoClient::saveSession())
	// is ever written to disk.
	QDialog *dialog = new QDialog(this);
	dialog->setWindowTitle("Sign in to Suunto");
	dialog->setModal(true);

	QVBoxLayout *outer = new QVBoxLayout(dialog);

	QFormLayout *formLayout = new QFormLayout;
	emailEdit_ = new QLineEdit;
	emailEdit_->setPlaceholderText("you@example.com");
	passwordEdit_ = new QLineEdit;
	passwordEdit_->setEchoMode(QLineEdit::Password);
	formLayout->addRow("Email:", emailEdit_);
	formLayout->addRow("Password:", passwordEdit_);
	outer->addLayout(formLayout);

	dialogStatusLabel_ = new QLabel;
	dialogStatusLabel_->setWordWrap(true);
	outer->addWidget(dialogStatusLabel_);

	QDialogButtonBox *buttons = new QDialogButtonBox;
	dialogSignInButton_ = buttons->addButton("Sign in", QDialogButtonBox::AcceptRole);
	buttons->addButton(QDialogButtonBox::Cancel);
	connect(dialogSignInButton_, &QPushButton::clicked, this, &MainWindow::onDialogSignInClicked);
	connect(passwordEdit_, &QLineEdit::returnPressed, this, &MainWindow::onDialogSignInClicked);
	connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
	outer->addWidget(buttons);

	return dialog;
}

QWidget *MainWindow::buildMainPage()
{
	QWidget *page = new QWidget;
	QVBoxLayout *layout = new QVBoxLayout(page);

	selectAllCheck_ = new QCheckBox("Select all");
	selectAllCheck_->setChecked(true);
	connect(selectAllCheck_, &QCheckBox::toggled, this, &MainWindow::onSelectAllToggled);
	layout->addWidget(selectAllCheck_);

	diveTable_ = new QTableWidget(0, 4);
	diveTable_->setHorizontalHeaderLabels({"", "Date", "Duration", "Max depth"});
	diveTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
	diveTable_->setSortingEnabled(true);
	diveTable_->verticalHeader()->setVisible(false);
	diveTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
	diveTable_->setSelectionMode(QAbstractItemView::NoSelection);
	layout->addWidget(diveTable_);

	QButtonGroup *modeGroup = new QButtonGroup(page);
	exportFilesMode_ = new QRadioButton("Export to files");
	directImportMode_ = new QRadioButton("Import directly into Subsurface");
	exportFilesMode_->setChecked(true);
	modeGroup->addButton(exportFilesMode_);
	modeGroup->addButton(directImportMode_);
	connect(exportFilesMode_, &QRadioButton::toggled, this, &MainWindow::onModeChanged);
	QHBoxLayout *modeRow = new QHBoxLayout;
	modeRow->addWidget(exportFilesMode_);
	modeRow->addWidget(directImportMode_);
	modeRow->addStretch();
	layout->addLayout(modeRow);

	exportFilesRow_ = new QWidget;
	QHBoxLayout *outRow = new QHBoxLayout(exportFilesRow_);
	outRow->setContentsMargins(0, 0, 0, 0);
	outRow->addWidget(new QLabel("Output directory:"));
	outDirEdit_ = new QLineEdit;
	outRow->addWidget(outDirEdit_);
	QPushButton *browseButton = new QPushButton("Browse...");
	connect(browseButton, &QPushButton::clicked, this, &MainWindow::onBrowseOutDir);
	outRow->addWidget(browseButton);
	layout->addWidget(exportFilesRow_);

	alsoFitCheck_ = new QCheckBox("Also save the raw, unmodified .fit for each dive (not needed for "
				       "Subsurface import)");
	layout->addWidget(alsoFitCheck_);

	directImportRow_ = new QWidget;
	QHBoxLayout *logRow = new QHBoxLayout(directImportRow_);
	logRow->setContentsMargins(0, 0, 0, 0);
	logRow->addWidget(new QLabel("Subsurface dive log:"));
	logPathEdit_ = new QLineEdit;
	logPathEdit_->setPlaceholderText("an existing .ssrf/.xml file, or a git-backed dive log folder");
	logRow->addWidget(logPathEdit_);
	QPushButton *browseFileButton = new QPushButton("Browse file...");
	connect(browseFileButton, &QPushButton::clicked, this, &MainWindow::onBrowseLogFile);
	logRow->addWidget(browseFileButton);
	QPushButton *browseFolderButton = new QPushButton("Browse folder...");
	connect(browseFolderButton, &QPushButton::clicked, this, &MainWindow::onBrowseLogFolder);
	logRow->addWidget(browseFolderButton);
	layout->addWidget(directImportRow_);
	directImportRow_->setVisible(false);

	QHBoxLayout *exportRow = new QHBoxLayout;
	exportButton_ = new QPushButton("Export selected dives");
	connect(exportButton_, &QPushButton::clicked, this, &MainWindow::onExportClicked);
	exportRow->addWidget(exportButton_);
	progressBar_ = new QProgressBar;
	exportRow->addWidget(progressBar_);
	layout->addLayout(exportRow);

	logView_ = new QPlainTextEdit;
	logView_->setReadOnly(true);
	logView_->setMaximumHeight(120);
	layout->addWidget(logView_);

	return page;
}

void MainWindow::setLoginPageBusy(bool busy)
{
	signInButton_->setEnabled(!busy);
}

void MainWindow::setLoginDialogBusy(bool busy)
{
	dialogSignInButton_->setEnabled(!busy);
	emailEdit_->setEnabled(!busy);
	passwordEdit_->setEnabled(!busy);
}

void MainWindow::onOpenLoginDialog()
{
	dialogStatusLabel_->clear();
	passwordEdit_->clear();
	setLoginDialogBusy(false);
	loginDialog_->open();
	emailEdit_->setFocus();
}

void MainWindow::onDialogSignInClicked()
{
	QString email = emailEdit_->text().trimmed();
	QString password = passwordEdit_->text();
	if (email.isEmpty() || password.isEmpty()) {
		dialogStatusLabel_->setText("Enter both email and password.");
		return;
	}
	firstLoginAttempt_ = false;
	setLoginDialogBusy(true);
	dialogStatusLabel_->setText("Signing in...");
	QMetaObject::invokeMethod(worker_, "login", Qt::QueuedConnection, Q_ARG(QString, sessionPath_),
				   Q_ARG(QString, email), Q_ARG(QString, password));
}

void MainWindow::onLoginSucceeded(const QString &email, bool usedCachedSession)
{
	setLoginPageBusy(false);
	setLoginDialogBusy(false);
	if (loginDialog_->isVisible()) {
		passwordEdit_->clear();
		loginDialog_->accept();
	}
	statusBar()->showMessage(usedCachedSession ? QString("Signed in as %1 (using saved session)").arg(email)
						    : QString("Signed in as %1").arg(email));
	stackedWidget_->setCurrentWidget(mainPage_);
	QMetaObject::invokeMethod(worker_, "fetchDiveList", Qt::QueuedConnection);
}

void MainWindow::onLoginFailed(const QString &error)
{
	setLoginPageBusy(false);
	setLoginDialogBusy(false);
	// The very first attempt silently probes for a saved session; a
	// failure there just means "no saved session yet", not a real error,
	// so just show the "Sign in" button rather than an error dialog.
	if (firstLoginAttempt_) {
		firstLoginAttempt_ = false;
		loginStatusLabel_->setText("No saved session found.");
		stackedWidget_->setCurrentWidget(loginPage_);
		return;
	}
	// A manual attempt from loginDialog_: keep it open and show the error
	// there so the user can correct and retry without reopening it.
	dialogStatusLabel_->setText(error);
}

void MainWindow::onDiveListReady(const QVector<QJsonObject> &dives)
{
	dives_ = dives;
	populateDiveTable();
	statusBar()->showMessage(QString("%1 dive(s) found").arg(dives.size()));
}

void MainWindow::onDiveListFailed(const QString &error)
{
	QMessageBox::warning(this, "Could not list dives", error);
}

void MainWindow::populateDiveTable()
{
	// Disable sorting while inserting rows (Qt's own recommendation --
	// otherwise each setItem() can trigger a re-sort mid-population) and
	// reset "select all" without re-entering onSelectAllToggled().
	diveTable_->setSortingEnabled(false);
	selectAllCheck_->blockSignals(true);
	selectAllCheck_->setChecked(true);
	selectAllCheck_->blockSignals(false);

	diveTable_->setRowCount(dives_.size());
	for (int row = 0; row < dives_.size(); ++row) {
		const QJsonObject &dive = dives_[row];
		QJsonObject ext = diveHeaderExtension(dive);

		NumericTableWidgetItem *checkItem = new NumericTableWidgetItem;
		checkItem->setCheckState(Qt::Checked);
		checkItem->setData(SortKeyRole, static_cast<int>(checkItem->checkState()));
		checkItem->setData(DiveIndexRole, row);
		diveTable_->setItem(row, 0, checkItem);

		qint64 startMs = dive["startTime"].toVariant().toLongLong();
		QString dateStr = QDateTime::fromMSecsSinceEpoch(startMs).toString("yyyy-MM-dd HH:mm");
		NumericTableWidgetItem *dateItem = new NumericTableWidgetItem(dateStr);
		dateItem->setData(SortKeyRole, static_cast<double>(startMs));
		diveTable_->setItem(row, 1, dateItem);

		// diveTime (bottom time from the dive computer) is null on some
		// older dives; fall back to the workout's overall totalTime.
		double durationSeconds = !ext["diveTime"].isNull() ? ext["diveTime"].toDouble()
								    : dive["totalTime"].toDouble();
		QString durationStr = durationSeconds > 0 ? formatDuration(durationSeconds) : "-";
		NumericTableWidgetItem *durationItem = new NumericTableWidgetItem(durationStr);
		durationItem->setData(SortKeyRole, durationSeconds);
		diveTable_->setItem(row, 2, durationItem);

		bool hasDepth = ext.contains("maxDepth");
		QString depthStr = hasDepth ? QString("%1 m").arg(ext["maxDepth"].toDouble(), 0, 'f', 1) : "-";
		NumericTableWidgetItem *depthItem = new NumericTableWidgetItem(depthStr);
		depthItem->setData(SortKeyRole, hasDepth ? ext["maxDepth"].toDouble() : -1.0);
		diveTable_->setItem(row, 3, depthItem);
	}

	diveTable_->setSortingEnabled(true);
}

void MainWindow::onBrowseOutDir()
{
	QString dir = QFileDialog::getExistingDirectory(this, "Choose output directory", outDirEdit_->text());
	if (!dir.isEmpty())
		outDirEdit_->setText(dir);
}

void MainWindow::onBrowseLogFile()
{
	QString file = QFileDialog::getOpenFileName(this, "Choose Subsurface dive log",
						     logPathEdit_->text(), "Subsurface logs (*.ssrf *.xml);;All files (*)");
	if (!file.isEmpty())
		logPathEdit_->setText(file);
}

void MainWindow::onBrowseLogFolder()
{
	QString dir = QFileDialog::getExistingDirectory(this, "Choose git-backed Subsurface dive log folder",
							 logPathEdit_->text());
	// Subsurface's git-backed storage is addressed as "path[branch]", not a
	// plain directory path (see core/git-access.cpp's is_git_repository());
	// "master" is the branch every local ("no cloud") git repo is created
	// and read/written on (see mobile-widgets/qmlmanager.cpp's
	// nocloud_localstorage()).
	if (!dir.isEmpty())
		logPathEdit_->setText(dir.endsWith(']') ? dir : dir + "[master]");
}

void MainWindow::onModeChanged()
{
	bool exportMode = exportFilesMode_->isChecked();
	exportFilesRow_->setVisible(exportMode);
	alsoFitCheck_->setVisible(exportMode);
	directImportRow_->setVisible(!exportMode);
	exportButton_->setText(exportMode ? "Export selected dives" : "Import selected dives into Subsurface");
}

void MainWindow::onSelectAllToggled(bool checked)
{
	for (int row = 0; row < diveTable_->rowCount(); ++row) {
		if (QTableWidgetItem *item = diveTable_->item(row, 0))
			item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
	}
}

void MainWindow::onExportClicked()
{
	// Sorting can put dives in a different visual row order than
	// dives_/SuuntoWorker::dives_, so look up each checked row's original
	// dive index (DiveIndexRole) rather than using the row number itself.
	QVector<int> indices;
	for (int row = 0; row < diveTable_->rowCount(); ++row) {
		QTableWidgetItem *item = diveTable_->item(row, 0);
		if (item && item->checkState() == Qt::Checked)
			indices.append(item->data(DiveIndexRole).toInt());
	}
	if (indices.isEmpty()) {
		QMessageBox::information(this, "Nothing selected", "Select at least one dive.");
		return;
	}

	exportButton_->setEnabled(false);
	progressBar_->setRange(0, indices.size());
	progressBar_->setValue(0);
	logView_->clear();

	if (exportFilesMode_->isChecked()) {
		QString outDir = outDirEdit_->text().trimmed();
		if (outDir.isEmpty() || !QDir(outDir).exists()) {
			QMessageBox::warning(this, "Invalid output directory",
					     "Choose an existing output directory first.");
			exportButton_->setEnabled(true);
			return;
		}
		QMetaObject::invokeMethod(worker_, "exportDives", Qt::QueuedConnection, Q_ARG(QVector<int>, indices),
					   Q_ARG(QString, outDir), Q_ARG(bool, alsoFitCheck_->isChecked()));
	} else {
		QString logPath = logPathEdit_->text().trimmed();
		// Git-backed logs are addressed as "path[branch]" (see
		// onBrowseLogFolder()); strip that suffix to check the underlying
		// path actually exists on disk.
		QString existsCheckPath = logPath;
		int bracket = existsCheckPath.indexOf('[');
		if (bracket >= 0)
			existsCheckPath.truncate(bracket);
		if (logPath.isEmpty() || !QFileInfo::exists(existsCheckPath)) {
			QMessageBox::warning(this, "Invalid dive log",
					     "Choose an existing Subsurface dive log file or folder first.");
			exportButton_->setEnabled(true);
			return;
		}
		QMetaObject::invokeMethod(worker_, "importDirectly", Qt::QueuedConnection,
					   Q_ARG(QVector<int>, indices), Q_ARG(QString, logPath));
	}
}

void MainWindow::onExportProgress(int current, int total, const QString &message)
{
	progressBar_->setRange(0, total);
	progressBar_->setValue(current - 1);
	logView_->appendPlainText(message);
}

void MainWindow::onExportFailed(int index, const QString &error)
{
	logView_->appendPlainText(QString("dive #%1: ERROR: %2").arg(index + 1).arg(error));
}

void MainWindow::onExportFinished(int succeeded, int total)
{
	progressBar_->setValue(total);
	exportButton_->setEnabled(true);
	logView_->appendPlainText(QString("done: %1 of %2 dive(s) exported").arg(succeeded).arg(total));
}

void MainWindow::onDirectImportFinished(int importedCount, const QString &error)
{
	exportButton_->setEnabled(true);
	if (!error.isEmpty()) {
		progressBar_->setValue(0);
		logView_->appendPlainText("ERROR: " + error);
		QMessageBox::warning(this, "Import failed", error);
		return;
	}
	progressBar_->setValue(progressBar_->maximum());
	logView_->appendPlainText(QString("done: %1 dive(s) imported into Subsurface").arg(importedCount));
}
