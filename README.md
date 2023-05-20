# ScreenCapture

## Dependencies 
#### Libraries
The project requires tesseract and leptonica libraries.  
The easiest way is to install them via `vcpkg`  

To install Tesseract library (x64 Windows):  
`./vcpkg install tesseract:x64-windows`  

To automatically link against the installed libraries:  
`./vcpkg integrate install`  


#### OCR Language model:  
The application requires loading a language model at runtime.  
To include the required language model folder in the executable directory during the build process, follow these steps:  
1. Go to `{Project}` -> `Properties` -> `Build Events` -> `Post-Build Event`.
2. In the `Command Line` field, add the following command:  
  `echo d | xcopy "$(SolutionDir)tessdata" "$(TargetDir)tessdata" /s /y`


Done.

## Controls:
Shortcut: `Ctrl` + `Win` + `S`  

Press `Esc` to exit selection without capturing the text  

You can exit the application via the system tray  
