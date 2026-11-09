// SPDX-License-Identifier: MIT
package io.github.protermuxarch.exynosprobe;

final class Native {
    static { System.loadLibrary("exynospresent"); }
    static native String configure(String nativeDir, boolean custom);
    static native String swapchainTest(String driver, android.view.Surface surface, String log);
    static native String swapchainPhase();
}
