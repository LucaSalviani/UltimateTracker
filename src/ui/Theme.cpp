#include "ui/Theme.h"

#include <QStringList>

namespace theme {

QFont uiFont(qreal pointSize, int weight)
{
    QFont f;
    f.setFamilies({QStringLiteral("Segoe UI Variable Text"), QStringLiteral("Segoe UI"),
                   QStringLiteral("Inter"), QStringLiteral("Noto Sans"), QStringLiteral("sans-serif")});
    f.setPointSizeF(pointSize);
    f.setWeight(QFont::Weight(weight));
    return f;
}

} // namespace theme
