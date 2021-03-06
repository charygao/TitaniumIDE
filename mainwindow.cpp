#include <QtWidgets/QShortcut>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QFileDialog>
#include <QtCore/QTextStream>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QInputDialog>
#include <QXmlStreamWriter>
#include <QDebug>
#include <QDialogButtonBox>
#include <QStandardPaths>
#include <QFileInfo>
#include <QCloseEvent>

QString projectDirectory;
QString projectName;
bool isProject=false;
struct TitaniumFile;
QHash<QString, TitaniumFile> files;
MainWindow::MainWindow(QWidget *p) : QMainWindow(p), ui(new Ui::MainWindow) {
    ui->setupUi(this);

    /* register the connections */
    connect(ui->fileButton, &QToolButton::clicked, this, &MainWindow::fileMenuClicked);

    /* populate the shortcuts */
    QShortcut *KBopenShortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_O), this);
    QShortcut *KBsaveShortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_S), this);
    QShortcut *KBnewDocShortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_N), this);

    KBopenShortcut->setAutoRepeat(false);
    KBsaveShortcut->setAutoRepeat(false);
    KBnewDocShortcut->setAutoRepeat(false);

    connect(KBopenShortcut, &QShortcut::activated, this, &MainWindow::openDialog);
    connect(KBsaveShortcut, &QShortcut::activated, this, &MainWindow::saveProject);
    connect(KBnewDocShortcut, &QShortcut::activated, this, &MainWindow::newDocument);
    ui->tabWidget->clear();
    ui->tabWidget->tabBar()->setDrawBase(false);

    QFile data(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/quit.tiQ");
    if(data.open(QIODevice::ReadOnly)) {
        QTextStream instream(&data);
        QString line2 = instream.readAll();
        data.close();
        if(line2 !="") {
            openProject(line2);
        }
    }
}

MainWindow::~MainWindow() {
    delete ui;
}
void MainWindow::fileMenuClicked() {
    QMenu *menu=new QMenu();
    menu->setObjectName("fileMenu");
    menu->setStyleSheet("#fileMenu{ background-color: rgb(88, 88, 88); border:none; color:white; width:200px;} \n #fileMenu::item:selected {color:white; background-color: rgb(12, 94, 176); }");
    QPoint p = ui->fileButton->pos();

    QRect widgetRect = ui->centralwidget->geometry();
    widgetRect.moveTopLeft(ui->centralwidget->parentWidget()->mapToGlobal(widgetRect.topLeft()));

    menu->move(widgetRect.topLeft().x() + p.x(), widgetRect.topLeft().y()+ ui->toolbar->size().height()-2);
    QAction *newAction = menu->addAction("New File");
    connect(newAction, &QAction::triggered, this, &MainWindow::newDocument);

    QAction *openAction =  menu->addAction("Open File");
    connect(openAction, &QAction::triggered, this, &MainWindow::openDialog);

    QAction *renameAction =  menu->addAction("Rename File");
    connect(renameAction, &QAction::triggered, this, &MainWindow::renameFileDialog);

    QAction *saveAction =  menu->addAction("Save File");
    connect(saveAction, &QAction::triggered, this, &MainWindow::save);

    menu->addSeparator();

    QAction *newProject = menu->addAction("New Project");
    connect(newProject, &QAction::triggered, this, &MainWindow::newProject);

    QAction *projectOpen =  menu->addAction("Open Project");
    connect(projectOpen, &QAction::triggered, this, &MainWindow::projectDialog);
    if(isProject) {
        QAction *saveAction = menu->addAction("Save Project");
        connect(saveAction, &QAction::triggered, this, &MainWindow::saveProject);

        QAction *closeAction = menu->addAction("Close Project");
        connect(closeAction, &QAction::triggered, this, &MainWindow::closeProject);
    } else {
        QAction *saveAction = menu->addAction("Save All");
        connect(saveAction, &QAction::triggered, this, &MainWindow::saveProject);

        QAction *closeAction = menu->addAction("Close All");
        connect(closeAction, &QAction::triggered, this, &MainWindow::closeProject);
    }
    menu->show();

}

void MainWindow::save() {
    int currentIndex = ui->tabWidget->currentIndex();
    QString text = ui->tabWidget->widget(currentIndex)->findChild<QTextEdit*>()->toPlainText()+"";
    if(ui->tabWidget->tabText(currentIndex).contains("*")) {
        if(ui->tabWidget->tabToolTip(currentIndex) == "New Document") {
            if(isProject) {
                renameFileDialog();
                saveFile(ui->tabWidget->tabToolTip(currentIndex), text);
            } else {
                QFileDialog *select = new QFileDialog();
                QString line;
                select->setLabelText(QFileDialog::Accept, QString("Save"));
                QString fileLocation = select->getSaveFileName(this,"Save","","TI-84+CE Programming (*.asm *.c *.txt *.h *.inc)");
                saveFile(fileLocation, text);
            }
        } else {
            saveFile(ui->tabWidget->tabToolTip(currentIndex), text);
        }
    }
}
void MainWindow::saveFile(QString location, QString contents) {
    QFile data(location);
    QFileInfo fileInfo(data);
    renameFile(fileInfo.fileName());
    if (data.open(QIODevice::WriteOnly | QIODevice::Text)){
        QTextStream stream(&data);
        stream << contents;
    }
}
void MainWindow::openDialog() {
    QFileDialog  *select = new QFileDialog();
    QString fileLocation = select->getOpenFileName(this,"Open","","TI-84+CE Programming (*.asm *.c *.txt *.h *.inc)");
    QFile file(fileLocation);
    if(file.open(QIODevice::ReadOnly)) {
        QTextStream instream(&file);
        QString line = instream.readAll();
        if(isProject) {
            QFileInfo fileInfo(file.fileName());
            QString filename(fileInfo.fileName());
            QFile data(projectDirectory+filename);
            if (data.open(QIODevice::WriteOnly | QIODevice::Text)){
                QTextStream stream(&data);
                stream << line;
            }
            fileLocation=projectDirectory+filename;
        }
        file.close();
        addFile(fileLocation, line,true);
    }
}
int MainWindow::addFile(QString fileLocation, QString line, bool saved) {
    QString filename;
    QString line2;
    bool openFile = true;
    int i =0;
    for(int c=0; c<(ui->tabWidget->count()); c++) {
        if(ui->tabWidget->tabToolTip(c)==fileLocation) {
            openFile=false;
            ui->tabWidget->setCurrentIndex(c);
            break;
        }
    }
    if(openFile || fileLocation=="New Document") {

        QTabWidget *tabs = ui->tabWidget;
        QWidget *newTab = new QWidget();
        QTextEdit *textEdit = new QTextEdit();
        newTab->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        textEdit->setStyleSheet("color:white; border:none;");
        textEdit->setFocusPolicy(Qt::ClickFocus);
        textEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        QLayout* layout = new QVBoxLayout(newTab);
        layout->setSpacing(0);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(textEdit);

        if(fileLocation=="New Document") {
            textEdit->setText(line);
            QString tabTitle = "New Document";
            if(!saved) {
                tabTitle+="*";
            }
            i = tabs->addTab(newTab, tabTitle);
            tabs->setTabToolTip(i, "New Document");
        } else {
            QFile file(fileLocation);
            QFileInfo fileInfo(file.fileName());
            filename = fileInfo.fileName();
            QString tabTitle = filename;
            if(!saved) {
                tabTitle+="*";
            }
            i = tabs->addTab(newTab, tabTitle);
            tabs->setTabToolTip(i, fileLocation);
            if(file.open(QIODevice::ReadOnly)) {
                QTextStream instream(&file);
                line2 = instream.readAll();
                file.close();
                textEdit->setText(line2);
            }
        }
    } else {
        i=-1;
    }
    ui->tabWidget->setCurrentIndex(i);
    TitaniumFile t;
    t.fileName = filename;
    t.filePath = fileLocation;
    t.saved = saved;
    t.text = line2;
    files.insert(fileLocation, t);
    return i;
}

void MainWindow::newDocument() {
    addFile("New Document","",false);
}

void MainWindow::newProject() {
    QTabWidget *tabs = ui->tabWidget;
    tabs->clear();


    bool ok;
    QString text = QInputDialog::getText(ui->centralwidget, tr("Name your project"),
                                         tr("Project Name: "), QLineEdit::Normal,
                                         "MyProject", &ok);
    if (ok && !text.isEmpty()) {

        QFileDialog *select = new QFileDialog();
        QString line;
        select->setLabelText(QFileDialog::Accept, QString("Project Location"));
        QString fileLocation = select->getExistingDirectory(this,"Project Location");
        if(fileLocation !="") {
            projectDirectory=fileLocation+"/" +text+ "/";
            QDir dir(projectDirectory);
            if (!dir.exists()){
                dir.mkdir(".");
            }
            QFile file(fileLocation+"/" +text+ "/" + text + ".tIDE");
            if(file.open(QIODevice::WriteOnly)) {

                QXmlStreamWriter xmlWriter(&file);
                xmlWriter.setAutoFormatting(true);
                xmlWriter.writeStartDocument();

                xmlWriter.writeStartElement("general");
                xmlWriter.writeTextElement("pname", text);
                xmlWriter.writeEndElement();


                file.close();
                newDocument();
                isProject=true;
                projectName=text;
                MainWindow::setWindowTitle("Project " + projectName + " - TitaniumIDE");

            }
        }
    }
}
void MainWindow::saveProject() {
    QString projectLoc;

    if(isProject) {
        projectLoc = projectDirectory;
    } else {
        projectLoc = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +"/";
    }
    QFile file(projectLoc +"session.tIDE");
    file.open(QIODevice::WriteOnly);
    QXmlStreamWriter xmlWriter(&file);
    xmlWriter.setAutoFormatting(true);
    xmlWriter.writeStartDocument();
    xmlWriter.writeStartElement("project");
    for(int c=0; c<(ui->tabWidget->count()); c++) {
        xmlWriter.writeStartElement("file");
        xmlWriter.writeTextElement("saved", QString::number(ui->tabWidget->tabText(c).contains("*")));
        xmlWriter.writeTextElement("exists", "false");
        xmlWriter.writeTextElement("name",  ui->tabWidget->tabToolTip(c));
        xmlWriter.writeTextElement("text",  ui->tabWidget->widget(c)->findChild<QTextEdit*>()->toPlainText()+"");
        xmlWriter.writeTextElement("lang",  "null");
        xmlWriter.writeEndElement();
    }
    xmlWriter.writeStartElement("general");
    xmlWriter.writeTextElement("pname", "");
    xmlWriter.writeTextElement("currentfile", QString::number(ui->tabWidget->currentIndex()));
    xmlWriter.writeTextElement("directory", projectDirectory);
    xmlWriter.writeEndElement();
    xmlWriter.writeEndElement();
    file.close();
}
void MainWindow::openProject(QString fileAddress) {
    isProject=true;
    ui->tabWidget->clear();
    QFile xmlFile(fileAddress);
    if (!xmlFile.open(QFile::ReadOnly | QFile::Text))
    {

    }
    QXmlStreamReader xmlReader;
    xmlReader.setDevice(&xmlFile);

    int currentindex = 0;
    QString saved;
    QString name;
    QString text;
    QString lang;
    QString exists;
    //Parse the XML until we reach end of it
    while(!xmlReader.atEnd() && !xmlReader.hasError()) {
        // Read next element
        QXmlStreamReader::TokenType token = xmlReader.readNext();
        //If token is just StartDocument - go to next
        if(token == QXmlStreamReader::StartDocument) {
            continue;
        }
        //If token is StartElement - read it
        if(token == QXmlStreamReader::StartElement) {

            if(xmlReader.name() == "general") {
                continue;
            }

            if(xmlReader.name() == "pname") {
                projectName= xmlReader.readElementText();
                ui->centralwidget->setWindowTitle(projectName);
            }
            if(xmlReader.name()=="directory") {
                projectDirectory=xmlReader.readElementText();
            }
            if(xmlReader.name() == "currentfile") {
                currentindex = xmlReader.readElementText().toInt();
            }
            if(xmlReader.name() == "file") {
                continue;
            }
            if(xmlReader.name() == "exists") {
                exists = xmlReader.readElementText();
            }
            if(xmlReader.name() == "saved") {
                saved = xmlReader.readElementText();
            }
            if(xmlReader.name() == "name") {
                name = xmlReader.readElementText();
            }
            if(xmlReader.name() == "text") {
                text = xmlReader.readElementText();
            }
            if(xmlReader.name() == "lang") {
                lang = xmlReader.readElementText();
                QFile fileF(name);
                if(fileF.exists() || exists=="false"){

                    ui->tabWidget->setCurrentIndex(addFile(name,text,(saved == "1" ? true : false)));
                }
            }
        }
    }

    if(xmlReader.hasError()) {
        QMessageBox::critical(this,
                              "Project file corrupt!", "Unknown characters in file",
                              QMessageBox::Ok);
        return;
    }

    //close reader and flush file
    xmlReader.clear();
    xmlFile.close();
    ui->tabWidget->setCurrentIndex(currentindex);
}
void MainWindow::closeProject() {
    if(!isProject) {
        //destroy session
    }
    projectDirectory = "";
    projectName = "";
    ui->tabWidget->clear();
    isProject=false;
    //check if saved (later)

}

QString MainWindow::projectDialog() {
    QFileDialog  *select = new QFileDialog();
    QString fileLocation = select->getOpenFileName(this,"Open","","Titanium IDE Project File (*.tIDE)");
    openProject(fileLocation);
    return fileLocation;
}

void MainWindow::renameFileDialog() {
    QString text = QInputDialog::getText(this, tr("QInputDialog::getText()"),
                                         tr("New file name"), QLineEdit::Normal);
    if(!text.isEmpty()) {
        renameFile(text);
    }
    saveProject();
}
void MainWindow::renameFile(QString name) {
    if(name!="") {
        ui->tabWidget->setTabToolTip(ui->tabWidget->currentIndex(),projectDirectory+name);
        ui->tabWidget->setTabText(ui->tabWidget->currentIndex(),name);
    }
    saveProject();
}
void MainWindow::closeEvent (QCloseEvent *event)
{
    saveProject();

    QFile data(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/quit.tiQ");
    if (data.open(QIODevice::WriteOnly | QIODevice::Text)){
        QTextStream stream(&data);
        if(isProject) {
            stream << projectDirectory+"/"+projectName+".tIDE";
        } else {
            stream << "";
        }
    }
    data.close();
}

