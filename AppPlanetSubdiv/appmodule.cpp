#include "appmodule.h"
#include "glwidget.h"



void AppModule::init()
{
	init_done = true;
}

void AppModule::onPostRender()
{
}

void AppModule::onResize(int w, int h)
{
	viewportWidth = w;
	viewportHeight = h;
}

void AppModule::cleanup()
{
}
