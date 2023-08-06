package org.sugarchaincore.qt;

import android.os.Bundle;
import android.system.ErrnoException;
import android.system.Os;

import org.qtproject.qt5.android.bindings.QtActivity;

import java.io.File;

public class SugarchainQtActivity extends QtActivity
{
    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        final File sugarchainDir = new File(getFilesDir().getAbsolutePath() + "/.sugarchain");
        if (!sugarchainDir.exists()) {
            sugarchainDir.mkdir();
        }

        super.onCreate(savedInstanceState);
    }
}
