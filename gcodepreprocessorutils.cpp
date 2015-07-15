#include <QRegExp>
#include <QDebug>
#include <QVector3D>
#include "gcodepreprocessorutils.h"

/**
* Searches the command string for an 'f' and replaces the speed value
* between the 'f' and the next space with a percentage of that speed.
* In that way all speed values become a ratio of the provided speed
* and don't get overridden with just a fixed speed.
*/
QString GcodePreprocessorUtils::overrideSpeed(QString command, double speed)
{
    QRegExp re("F([0-9.]+)");

    if (re.indexIn(command) != -1) {
        command.replace(re, QString("F%1").arg(re.cap(1).toDouble() / 100 * speed));
    }

    return command;
}

/**
* Removes any comments within parentheses or beginning with a semi-colon.
*/
QString GcodePreprocessorUtils::removeComment(QString command)
{
    // Remove any comments within ( parentheses ) using regex "\([^\(]*\)"
    command.replace(QRegExp("\\(+[^\\(]*\\)+"), "");

    // Remove any comment beginning with ';' using regex ";.*"
    command.replace(QRegExp(";.*"), "");

    return command.trimmed();
}

/**
* Searches for a comment in the input string and returns the first match.
*/
QString GcodePreprocessorUtils::parseComment(QString command)
{
    // REGEX: Find any comment, includes the comment characters:
    // "(?<=\()[^\(\)]*|(?<=\;)[^;]*"
    // "(?<=\\()[^\\(\\)]*|(?<=\\;)[^;]*"

    QRegExp re("(\\([^\\(\\)]*\\)|;[^;].*)");

    if (re.indexIn(command) != -1) {
        return re.cap(1);
    }
    return "";
}

QString GcodePreprocessorUtils::truncateDecimals(int length, QString command)
{
    QRegExp re("(\\d*\\.\\d*)");
    int pos = 0;

    while ((pos = re.indexIn(command, pos)) != -1)
    {
        QString newNum = QString::number(re.cap(1).toDouble(), 'f', length);
        command = command.left(pos) + newNum + command.mid(pos + re.matchedLength());
        pos += newNum.length() + 1;
    }

    return command;
}

QString GcodePreprocessorUtils::removeAllWhitespace(QString command)
{
    return command.replace(QRegExp("\\s"),"");
}

QList<QString> GcodePreprocessorUtils::parseCodes(QList<QString> args, char code)
{
    QList<QString> l;
    char address = QChar(code).toUpper().toLatin1();

    foreach (QString s, args) {
        if (s.length() > 0 && s.at(0).toUpper().toLatin1() == address) l.append(s.mid(1));
    }

    return l;
}

QList<int> GcodePreprocessorUtils::parseGCodes(QString command)
{
    QRegExp re("[Gg]0*(\\d+)");

    QList<int> codes;
    int pos = 0;

    while ((pos = re.indexIn(command, pos)) != -1) {
        codes.append(re.cap(1).toInt());
        pos += re.matchedLength();
    }

    return codes;
}

QList<int> GcodePreprocessorUtils::parseMCodes(QString command)
{
    QRegExp re("[Mm]0*(\\d+)");

    QList<int> codes;
    int pos = 0;

    while ((pos = re.indexIn(command, pos)) != -1) {
        codes.append(re.cap(1).toInt());
        pos += re.matchedLength();
    }

    return codes;
}

/**
* Update a point given the arguments of a command.
*/
QVector3D GcodePreprocessorUtils::updatePointWithCommand(QString command, QVector3D initial, bool absoluteMode)
{
    QList<QString> l = splitCommand(command);
    return updatePointWithCommand(l, initial, absoluteMode);
}

/**
* Update a point given the arguments of a command, using a pre-parsed list.
*/
QVector3D GcodePreprocessorUtils::updatePointWithCommand(QList<QString> commandArgs, QVector3D initial, bool absoluteMode)
{

    double x = parseCoord(commandArgs, 'X');
    double y = parseCoord(commandArgs, 'Y');
    double z = parseCoord(commandArgs, 'Z');

    return updatePointWithCommand(initial, x, y, z, absoluteMode);
}

/**
* Update a point given the new coordinates.
*/
QVector3D GcodePreprocessorUtils::updatePointWithCommand(QVector3D initial, double x, double y, double z, bool absoluteMode)
{

    QVector3D newPoint(initial.x(), initial.y(), initial.z());

    if (absoluteMode) {
        if (!_isnan(x)) newPoint.setX(x);
        if (!_isnan(y)) newPoint.setY(y);
        if (!_isnan(z)) newPoint.setZ(z);
    } else {
        if (!_isnan(x)) newPoint.setX(newPoint.x() + x);
        if (!_isnan(y)) newPoint.setY(newPoint.y() + y);
        if (!_isnan(z)) newPoint.setZ(newPoint.z() + z);
    }

    return newPoint;
}

QVector3D GcodePreprocessorUtils::updateCenterWithCommand(QList<QString> commandArgs, QVector3D initial, QVector3D nextPoint, bool absoluteIJKMode, bool clockwise)
{
    double i = parseCoord(commandArgs, 'I');
    double j = parseCoord(commandArgs, 'J');
    double k = parseCoord(commandArgs, 'K');
    double radius = parseCoord(commandArgs, 'R');

    if (_isnan(i) && _isnan(j) && _isnan(k)) {
        return convertRToCenter(initial, nextPoint, radius, absoluteIJKMode, clockwise);
    }

    return updatePointWithCommand(initial, i, j, k, absoluteIJKMode);
}

QString GcodePreprocessorUtils::generateG1FromPoints(QVector3D start, QVector3D end, bool absoluteMode, int precision)
{
    QString sb("G1");

    if (absoluteMode) {
        if (!_isnan(end.x())) sb.append("X" + QString::number(end.x(), 'f', precision));
        if (!_isnan(end.y())) sb.append("Y" + QString::number(end.y(), 'f', precision));
        if (!_isnan(end.z())) sb.append("Z" + QString::number(end.z(), 'f', precision));
    } else {
        if (!_isnan(end.x())) sb.append("X" + QString::number(end.x() - start.x(), 'f', precision));
        if (!_isnan(end.y())) sb.append("Y" + QString::number(end.y() - start.y(), 'f', precision));
        if (!_isnan(end.z())) sb.append("Z" + QString::number(end.z() - start.z(), 'f', precision));
    }

    return sb;
}

///**
//* Splits a gcode command by each word/argument, doesn't care about spaces.
//* This command is about the same speed as the string.split(" ") command,
//* but might be a little faster using precompiled regex.
//*/
QList<QString> GcodePreprocessorUtils::splitCommand(QString command) {
    QList<QString> l;
    bool readNumeric = false;
    QString sb;

    for (int i = 0; i < command.length(); i++) {
        QChar c = command.at(i);

        if (readNumeric && !c.isDigit() && c != '.') {
            readNumeric = false;
            l.append(sb);
            sb = "";
            if (c.isLetter()) sb.append(c);
        } else if (c.isDigit() || c == '.' || c == '-') {
            sb.append(c);
            readNumeric = true;
        } else if (c.isLetter()) sb.append(c);
    }

    if (sb.length() > 0) l.append(sb);

    return l;
}

// TODO: Replace everything that uses this with a loop that loops through
// the string and creates a hash with all the values.
double GcodePreprocessorUtils::parseCoord(QList<QString> argList, char c)
{
    char address = QChar(c).toUpper().toLatin1();
    foreach (QString t, argList)
    {
        if (t.length() > 0 && t.at(0).toUpper().toLatin1() == address)
        {
            return t.mid(1).toDouble();
        }
    }
    return std::numeric_limits<double>::quiet_NaN();
}

//static public List<String> convertArcsToLines(Point3d start, Point3d end) {
//    List<String> l = new ArrayList<String>();

//    return l;
//}

QVector3D GcodePreprocessorUtils::convertRToCenter(QVector3D start, QVector3D end, double radius, bool absoluteIJK, bool clockwise) {
    double R = radius;
    QVector3D center;

    double x = end.x() - start.x();
    double y = end.y() - start.y();

    double h_x2_div_d = 4 * R * R - x * x - y * y;
    if (h_x2_div_d < 0) { qDebug() << "Error computing arc radius."; }
    h_x2_div_d = (-sqrt(h_x2_div_d)) / hypot(x, y);

    if (!clockwise) h_x2_div_d = -h_x2_div_d;

    // Special message from gcoder to software for which radius
    // should be used.
    if (R < 0) {
        h_x2_div_d = -h_x2_div_d;
        // TODO: Places that use this need to run ABS on radius.
        radius = -radius;
    }

    double offsetX = 0.5 * (x - (y * h_x2_div_d));
    double offsetY = 0.5 * (y + (x * h_x2_div_d));

    if (!absoluteIJK) {
        center.setX(start.x() + offsetX);
        center.setY(start.y() + offsetY);
    } else {
        center.setX(offsetX);
        center.setY(offsetY);
    }

    return center;
}

/**
* Return the angle in radians when going from start to end.
*/
double GcodePreprocessorUtils::getAngle(QVector3D start, QVector3D end) {
    double deltaX = end.x() - start.x();
    double deltaY = end.y() - start.y();

    double angle = 0.0;

    if (deltaX != 0) { // prevent div by 0
        // it helps to know what quadrant you are in
        if (deltaX > 0 && deltaY >= 0) { // 0 - 90
            angle = atan(deltaY / deltaX);
        } else if (deltaX < 0 && deltaY >= 0) { // 90 to 180
            angle = M_PI - abs(atan(deltaY / deltaX));
        } else if (deltaX < 0 && deltaY < 0) { // 180 - 270
            angle = M_PI + abs(atan(deltaY / deltaX));
        } else if (deltaX > 0 && deltaY < 0) { // 270 - 360
            angle = M_PI * 2 - abs(atan(deltaY / deltaX));
        }
    }
    else {
        // 90 deg
        if (deltaY > 0) {
            angle = M_PI / 2.0;
        }
        // 270 deg
        else {
            angle = M_PI * 3.0 / 2.0;
        }
    }

    return angle;
}

double GcodePreprocessorUtils::calculateSweep(double startAngle, double endAngle, bool isCw)
{
    double sweep;

    // Full circle
    if (startAngle == endAngle) {
        sweep = (M_PI * 2);
        // Arcs
    } else {
        // Account for full circles and end angles of 0/360
        if (endAngle == 0) {
            endAngle = M_PI * 2;
        }
        // Calculate distance along arc.
        if (!isCw && endAngle < startAngle) {
            sweep = ((M_PI * 2 - startAngle) + endAngle);
        } else if (isCw && endAngle > startAngle) {
            sweep = ((M_PI * 2 - endAngle) + startAngle);
        } else {
            sweep = abs(endAngle - startAngle);
        }
    }

    return sweep;
}

/**
* Generates the points along an arc including the start and end points.
*/
QList<QVector3D> GcodePreprocessorUtils::generatePointsAlongArcBDring(QVector3D start, QVector3D end, QVector3D center, bool clockwise, double R, double minArcLength, double arcSegmentLength)
{
    double radius = R;

    // Calculate radius if necessary.
    if (radius == 0) {
        radius = sqrt(pow((double)(start.x() - center.x()), 2.0) + pow((double)(end.y() - center.y()), 2.0));
    }

    double startAngle = getAngle(center, start);
    double endAngle = getAngle(center, end);
    double sweep = calculateSweep(startAngle, endAngle, clockwise);

    // Convert units.
    double arcLength = sweep * radius;

    // If this arc doesn't meet the minimum threshold, don't expand.
    if (minArcLength > 0 && arcLength < minArcLength) {
        QList<QVector3D> empty;
        return empty;
    }

    int numPoints = 20;

    if (arcSegmentLength <= 0 && minArcLength > 0) {
        arcSegmentLength = (sweep * radius) / minArcLength;
    }

    if (arcSegmentLength > 0) {
        numPoints = (int)ceil(arcLength/arcSegmentLength);
    }

    return generatePointsAlongArcBDring(start, end, center, clockwise, radius, startAngle, sweep, numPoints);
}

/**
* Generates the points along an arc including the start and end points.
*/
QList<QVector3D> GcodePreprocessorUtils::generatePointsAlongArcBDring(QVector3D p1, QVector3D p2,
                                                                      QVector3D center, bool isCw,
                                                                      double radius, double startAngle,
                                                                      double sweep, int numPoints)
{
    QVector3D lineEnd(p2.x(), p2.y(), p1.z());
    QList<QVector3D> segments;
    double angle;

    // Calculate radius if necessary.
    if (radius == 0) {
        radius = sqrt(pow((double)(p1.x() - center.x()), 2.0) + pow((double)(p1.y() - center.y()), 2.0));
    }

    double zIncrement = (p2.z() - p1.z()) / numPoints;
    for (int i = 1; i < numPoints; i++)
    {
        if (isCw) {
            angle = (startAngle - i * sweep / numPoints);
        } else {
            angle = (startAngle + i * sweep / numPoints);
        }

        if (angle >= M_PI * 2) {
            angle = angle - M_PI * 2;
        }

        lineEnd.setX(cos(angle) * radius + center.x());
        lineEnd.setY(sin(angle) * radius + center.y());
        lineEnd.setZ(lineEnd.z() + zIncrement);

        segments.append(lineEnd);
    }

    segments.append(p2);

    return segments;
}
