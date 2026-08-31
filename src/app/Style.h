#pragma once

#include <QString>

inline QString omstickerStyleSheet()
{
    return QStringLiteral(R"(
        QMainWindow, QMainWindow > QWidget {
            background: #05182e;
        }
        QWidget {
            color: #f6dcac;
            font-family: "Noto Sans", "Inter", sans-serif;
            font-size: 13px;
        }
        QLabel, QCheckBox {
            background: transparent;
        }
        QLabel#title {
            font-size: 22px;
            font-weight: 700;
            color: #faa968;
        }
        QLabel#subtitle {
            color: #8cbfb8;
            font-size: 13px;
        }
        QLabel#section {
            font-weight: 700;
            font-size: 11px;
            letter-spacing: 1px;
            color: #8cbfb8;
        }
        QLabel#hint {
            color: #3f8f8a;
            font-size: 12px;
        }
        QLabel#warning {
            color: #f85525;
            font-size: 12px;
        }
        QLabel#ok {
            color: #8cbfb8;
        }
        QFrame#card {
            background: #0a2540;
            border: 1px solid #134e5a;
            border-radius: 14px;
        }
        QLineEdit, QComboBox {
            background: #031222;
            color: #f6dcac;
            border: 1px solid #2a6b78;
            border-radius: 8px;
            padding: 8px 10px;
            min-height: 20px;
            selection-background-color: #134e5a;
        }
        QLineEdit:focus, QComboBox:focus {
            border: 1px solid #faa968;
        }
        QComboBox::drop-down {
            border: none;
            width: 24px;
        }
        QComboBox QAbstractItemView {
            background: #031222;
            color: #f6dcac;
            border: 1px solid #2a6b78;
            selection-background-color: #134e5a;
            outline: none;
        }
        QPushButton {
            background: #0a2540;
            color: #f6dcac;
            border: 1px solid #2a6b78;
            border-radius: 8px;
            padding: 8px 14px;
        }
        QPushButton:hover {
            border-color: #faa968;
            color: #faa968;
        }
        QPushButton:disabled {
            color: #2a6b78;
            border-color: #134e5a;
        }
        QPushButton#flash {
            background: #faa968;
            color: #031222;
            border: none;
            border-radius: 10px;
            font-weight: 700;
            font-size: 15px;
            min-height: 44px;
        }
        QPushButton#flash:hover {
            background: #e97b3c;
        }
        QPushButton#flash:disabled {
            background: #134e5a;
            color: #8cbfb8;
        }
        QCheckBox {
            spacing: 8px;
            color: #f6dcac;
        }
        QCheckBox::indicator {
            width: 16px;
            height: 16px;
            border-radius: 4px;
            border: 1px solid #2a6b78;
            background: #031222;
        }
        QCheckBox::indicator:checked {
            background: #faa968;
            border-color: #faa968;
        }
        QProgressBar {
            background: #031222;
            border: 1px solid #134e5a;
            border-radius: 8px;
            height: 14px;
            text-align: center;
            color: #f6dcac;
        }
        QProgressBar::chunk {
            background: #faa968;
            border-radius: 7px;
        }
    )");
}
