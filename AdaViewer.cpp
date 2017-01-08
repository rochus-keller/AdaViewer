/*
* Copyright 2012-2017 Rochus Keller <mailto:me@rochus-keller.info>
*
* This file is part of the AdaViewer application.
*
* The following is the license that applies to this copy of the
* application. For a license to use the application under conditions
* other than those described here, please email to me@rochus-keller.info.
*
* GNU General Public License Usage
* This file may be used under the terms of the GNU General Public
* License (GPL) versions 2.0 or 3.0 as published by the Free Software
* Foundation and appearing in the file LICENSE.GPL included in
* the packaging of this file. Please review the following information
* to ensure GNU General Public Licensing requirements will be met:
* http://www.fsf.org/licensing/licenses/info/GPLv2.html and
* http://www.gnu.org/copyleft/gpl.html.
*/

#include "AdaViewer.h"
#include <QApplication>
#include <QFileInfo>

AdaViewer::AdaViewer(QWidget *parent)
	: QMainWindow(parent)
{
	d_edit = new Ada::Editor(this);
	d_edit->installDefaultPopup();
	d_edit->setReadOnly(true);
	setCentralWidget( d_edit );

	showMaximized();

	connect( d_edit,SIGNAL(updateCaption(QString)), this, SLOT(onCaption(QString)) );

	setWindowTitle(tr("AdaViewer") );
}

AdaViewer::~AdaViewer()
{
	
}

void AdaViewer::open(const QString & path)
{
	d_edit->loadFromFile(path);
}

void AdaViewer::onCaption(const QString & path)
{
	QFileInfo info(path);
	setWindowTitle(tr("%1 - AdaViewer").arg(info.fileName() ) );
}

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);

	QString path;
	QStringList args = a.arguments();
	for( int i = 1; i < args.size(); i++ ) // arg 0 enthält Anwendungspfad
	{
		QString arg = args[ i ];
		if( arg[ 0 ] != '-' )
		{
			QFileInfo info( arg );
			const QByteArray suffix = info.suffix().toUpper().toLatin1();

			if( suffix == "ADB" || suffix == "ADS" )
			{
				path = arg;
			}
		}
	}

	AdaViewer w;
	if( !path.isEmpty() )
		w.open(path);
	w.show();

	return a.exec();
}
