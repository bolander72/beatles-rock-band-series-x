// BSD-3-Clause. Included inside the common EmulatorWindow::RunTitle entry.
// All title launch paths, including saved restarts and protocol activation,
// must establish the native-account/local-save association before guest boot.
#if XE_PLATFORM_WINRT
  if (!beatles::SignInConsoleAccounts(emulator_->kernel_state())) {
    return X_STATUS_ACCESS_DENIED;
  }
#endif
