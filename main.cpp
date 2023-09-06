#include "panel.h"

#include <mitkStandaloneDataStorage.h>

#include <QmitkRegisterClasses.h>

#include <QApplication>

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);

	QCoreApplication::setApplicationName("VolumeRenderingDemo");
	
	QmitkRegisterClasses();

	mitk::StandaloneDataStorage::Pointer datastorage = mitk::StandaloneDataStorage::New();

	Panel panel;
	panel.SetDataStorage(datastorage);
	panel.show();

	return app.exec();
}
