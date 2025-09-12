/*
    Simple test to verify OpenGL rendering works
*/

#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <unistd.h>

int main() {
    Display* display = XOpenDisplay(NULL);
    if (!display) {
        printf("Failed to open X display\n");
        return 1;
    }
    
    int screen = DefaultScreen(display);
    
    // Request double buffered, RGBA, depth buffer
    int attribs[] = {
        GLX_RGBA,
        GLX_DOUBLEBUFFER,
        GLX_DEPTH_SIZE, 24,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        None
    };
    
    XVisualInfo* vi = glXChooseVisual(display, screen, attribs);
    if (!vi) {
        printf("No suitable visual found\n");
        return 1;
    }
    
    // Create window
    Window root = RootWindow(display, screen);
    XSetWindowAttributes swa = {0};
    swa.colormap = XCreateColormap(display, root, vi->visual, AllocNone);
    swa.event_mask = ExposureMask | KeyPressMask;
    
    Window window = XCreateWindow(
        display, root,
        0, 0, 800, 600,
        0, vi->depth, InputOutput, vi->visual,
        CWColormap | CWEventMask, &swa
    );
    
    XMapWindow(display, window);
    XStoreName(display, window, "OpenGL Test");
    
    // Create GL context
    GLXContext ctx = glXCreateContext(display, vi, NULL, GL_TRUE);
    glXMakeCurrent(display, window, ctx);
    
    printf("OpenGL Version: %s\n", glGetString(GL_VERSION));
    printf("OpenGL Vendor: %s\n", glGetString(GL_VENDOR));
    
    // Simple render loop
    for (int i = 0; i < 300; i++) {
        // Clear to red
        glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        // Draw a white triangle
        glBegin(GL_TRIANGLES);
        glColor3f(1.0f, 1.0f, 1.0f);
        glVertex2f(-0.5f, -0.5f);
        glVertex2f(0.5f, -0.5f);
        glVertex2f(0.0f, 0.5f);
        glEnd();
        
        glXSwapBuffers(display, window);
        usleep(16666); // ~60 FPS
    }
    
    printf("Test completed successfully\n");
    
    glXMakeCurrent(display, None, NULL);
    glXDestroyContext(display, ctx);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    
    return 0;
}