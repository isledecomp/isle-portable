# Methods called from native code via JNI (see ISLE/android/filepicker.cpp)
-keep class org.legoisland.isle.IsleActivity {
    java.lang.String importGameFiles(java.lang.String);
}
