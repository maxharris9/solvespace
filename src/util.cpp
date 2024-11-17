//-----------------------------------------------------------------------------
// Utility functions, mostly various kinds of vector math (working on real
// numbers, not working on quantities in the symbolic algebra system).
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

void SolveSpace::AssertFailure(const char *file, unsigned line, const char *function,
                               const char *condition, const char *message) {
    std::string formattedMsg;
    formattedMsg += ssprintf("File %s, line %u, function %s:\n", file, line, function);
    formattedMsg += ssprintf("Assertion failed: %s.\n", condition);
    formattedMsg += ssprintf("Message: %s.\n", message);
    SolveSpace::Platform::FatalError(formattedMsg);
}

std::string SolveSpace::ssprintf(const char *fmt, ...)
{
    va_list va;

    va_start(va, fmt);
    int size = vsnprintf(NULL, 0, fmt, va);
    ssassert(size >= 0, "vsnprintf could not encode string");
    va_end(va);

    std::string result;
    result.resize(size + 1);

    va_start(va, fmt);
    vsnprintf(&result[0], size + 1, fmt, va);
    va_end(va);

    result.resize(size);
    return result;
}

char32_t utf8_iterator::operator*()
{
    const uint8_t *it = (const uint8_t*) this->p;
    char32_t result = *it;

    if((result & 0x80) != 0) {
      unsigned int mask = 0x40;

      do {
        result <<= 6;
        unsigned int c = (*++it);
        mask   <<= 5;
        result  += c - 0x80;
      } while((result & mask) != 0);

      result &= mask - 1;
    }

    this->n = (const char*) (it + 1);
    return result;
}

int64_t SolveSpace::GetMilliseconds()
{
    auto timestamp = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(timestamp).count();
}

void SolveSpace::MakeMatrix(double *mat,
                            double a11, double a12, double a13, double a14,
                            double a21, double a22, double a23, double a24,
                            double a31, double a32, double a33, double a34,
                            double a41, double a42, double a43, double a44)
{
    mat[ 0] = a11;
    mat[ 1] = a21;
    mat[ 2] = a31;
    mat[ 3] = a41;
    mat[ 4] = a12;
    mat[ 5] = a22;
    mat[ 6] = a32;
    mat[ 7] = a42;
    mat[ 8] = a13;
    mat[ 9] = a23;
    mat[10] = a33;
    mat[11] = a43;
    mat[12] = a14;
    mat[13] = a24;
    mat[14] = a34;
    mat[15] = a44;
}

void SolveSpace::MultMatrix(double *mata, double *matb, double *matr) {
    for(int i = 0; i < 4; i++) {
        for(int j = 0; j < 4; j++) {
            double s = 0.0;
            for(int k = 0; k < 4; k++) {
                s += mata[k * 4 + j] * matb[i * 4 + k];
            }
           matr[i * 4 + j] = s;
        }
    }
}

//-----------------------------------------------------------------------------
// Format the string for our message box appropriately, and then display
// that string.
//-----------------------------------------------------------------------------
static void MessageBox(const char *fmt, va_list va, bool error,
                       std::function<void()> onDismiss = std::function<void()>())
{
    va_list va_size;
    va_copy(va_size, va);
    int size = vsnprintf(NULL, 0, fmt, va_size);
    ssassert(size >= 0, "vsnprintf could not encode string");
    va_end(va_size);

    std::string text;
    text.resize(size);

    vsnprintf(&text[0], size + 1, fmt, va);

    // Split message text using a heuristic for better presentation.
    size_t separatorAt = 0;
    while(separatorAt != std::string::npos) {
        size_t dotAt = text.find('.', separatorAt + 1),
               colonAt = text.find(':', separatorAt + 1);
        separatorAt = min(dotAt, colonAt);
        if(separatorAt == std::string::npos ||
                (separatorAt + 1 < text.size() && isspace(text[separatorAt + 1]))) {
            break;
        }
    }
    std::string message = text;
    std::string description;
    if(separatorAt != std::string::npos) {
        message = text.substr(0, separatorAt + 1);
        if(separatorAt + 1 < text.size()) {
            description = text.substr(separatorAt + 1);
        }
    }

    if(description.length() > 0) {
        std::string::iterator it = description.begin();
        while(isspace(*it)) it++;
        description = description.substr(it - description.begin());
    }

    Platform::MessageDialogRef dialog = CreateMessageDialog(SS.GW.window);
    if (!dialog) {
        if (error) {
            fprintf(stderr, "Error: %s\n", message.c_str());
        } else {
            fprintf(stderr, "Message: %s\n", message.c_str());
        }
        if(onDismiss) {
            onDismiss();
        }
        return;
    }
    using Platform::MessageDialog;
    if(error) {
        dialog->SetType(MessageDialog::Type::ERROR);
    } else {
        dialog->SetType(MessageDialog::Type::INFORMATION);
    }
    dialog->SetTitle(error ? C_("title", "Error") : C_("title", "Message"));
    dialog->SetMessage(message);
    if(!description.empty()) {
        dialog->SetDescription(description);
    }
    dialog->AddButton(C_("button", "&OK"), MessageDialog::Response::OK,
                      /*isDefault=*/true);

    dialog->onResponse = [=](MessageDialog::Response _response) {
        if(onDismiss) {
            onDismiss();
        }
    };
    dialog->ShowModal();
}
void SolveSpace::Error(const char *fmt, ...)
{
    va_list f;
    va_start(f, fmt);
    MessageBox(fmt, f, /*error=*/true);
    va_end(f);
}
void SolveSpace::Message(const char *fmt, ...)
{
    va_list f;
    va_start(f, fmt);
    MessageBox(fmt, f, /*error=*/false);
    va_end(f);
}
void SolveSpace::MessageAndRun(std::function<void()> onDismiss, const char *fmt, ...)
{
    va_list f;
    va_start(f, fmt);
    MessageBox(fmt, f, /*error=*/false, onDismiss);
    va_end(f);
}

//-----------------------------------------------------------------------------
// Solve a mostly banded matrix. In a given row, there are LEFT_OF_DIAG
// elements to the left of the diagonal element, and RIGHT_OF_DIAG elements to
// the right (so that the total band width is LEFT_OF_DIAG + RIGHT_OF_DIAG + 1).
// There also may be elements in the last two columns of any row. We solve
// without pivoting.
//-----------------------------------------------------------------------------
void BandedMatrix::Solve() {
    int i, ip, j, jp;
    double temp;

    // Reduce the matrix to upper triangular form.
    for(i = 0; i < n; i++) {
        for(ip = i+1; ip < n && ip <= (i + LEFT_OF_DIAG); ip++) {
            temp = A[ip][i]/A[i][i];

            for(jp = i; jp < (n - 2) && jp <= (i + RIGHT_OF_DIAG); jp++) {
                A[ip][jp] -= temp*(A[i][jp]);
            }
            A[ip][n-2] -= temp*(A[i][n-2]);
            A[ip][n-1] -= temp*(A[i][n-1]);

            B[ip] -= temp*B[i];
        }
    }

    // And back-substitute.
    for(i = n - 1; i >= 0; i--) {
        temp = B[i];

        if(i < n-1) temp -= X[n-1]*A[i][n-1];
        if(i < n-2) temp -= X[n-2]*A[i][n-2];

        for(j = min(n - 3, i + RIGHT_OF_DIAG); j > i; j--) {
            temp -= X[j]*A[i][j];
        }
        X[i] = temp / A[i][i];
    }
}
//#include "vector.cpp"

size_t VectorHash::operator()(const Vector &v) const {
    const size_t size = (size_t)pow(std::numeric_limits<size_t>::max(), 1.0 / 3.0) - 1;
    const double eps = 4.0 * LENGTH_EPS;

    double x = fabs(v.x) / eps;
    double y = fabs(v.y) / eps;
    double z = fabs(v.y) / eps;

    size_t xs = size_t(fmod(x, (double)size));
    size_t ys = size_t(fmod(y, (double)size));
    size_t zs = size_t(fmod(z, (double)size));

    return (zs * size + ys) * size + xs;
}

bool VectorPred::operator()(Vector a, Vector b) const {
    return a.Equals(b, LENGTH_EPS);
}

Vector4 Vector4::From(double w, double x, double y, double z) {
    Vector4 ret;
    ret.w = w;
    ret.x = x;
    ret.y = y;
    ret.z = z;
    return ret;
}

Vector4 Vector4::From(double w, Vector v) {
    return Vector4::From(w, w*v.x, w*v.y, w*v.z);
}

Vector4 Vector4::Blend(Vector4 a, Vector4 b, double t) {
    return (a.ScaledBy(1 - t)).Plus(b.ScaledBy(t));
}

Vector4 Vector4::Plus(Vector4 b) const {
    return Vector4::From(w + b.w, x + b.x, y + b.y, z + b.z);
}

Vector4 Vector4::Minus(Vector4 b) const {
    return Vector4::From(w - b.w, x - b.x, y - b.y, z - b.z);
}

Vector4 Vector4::ScaledBy(double s) const {
    return Vector4::From(w*s, x*s, y*s, z*s);
}

Vector Vector4::PerspectiveProject() const {
    return Vector::From(x / w, y / w, z / w);
}

Point2d Point2d::From(double x, double y) {
    return { x, y };
}

Point2d Point2d::FromPolar(double r, double a) {
    return { r * cos(a), r * sin(a) };
}

double Point2d::Angle() const {
    double a = atan2(y, x);
    return M_PI + remainder(a - M_PI, 2 * M_PI);
}

double Point2d::AngleTo(const Point2d &p) const {
    return p.Minus(*this).Angle();
}

Point2d Point2d::Plus(const Point2d &b) const {
    return { x + b.x, y + b.y };
}

Point2d Point2d::Minus(const Point2d &b) const {
    return { x - b.x, y - b.y };
}

Point2d Point2d::ScaledBy(double s) const {
    return { x * s, y * s };
}

double Point2d::DivProjected(Point2d delta) const {
    return (x*delta.x + y*delta.y) / (delta.x*delta.x + delta.y*delta.y);
}

double Point2d::MagSquared() const {
    return x*x + y*y;
}

double Point2d::Magnitude() const {
    return sqrt(x*x + y*y);
}

Point2d Point2d::WithMagnitude(double v) const {
    double m = Magnitude();
    if(m < 1e-20) {
        return { v, 0 };
    }
    return { x * v / m, y * v / m };
}

double Point2d::DistanceTo(const Point2d &p) const {
    double dx = x - p.x;
    double dy = y - p.y;
    return sqrt(dx*dx + dy*dy);
}

double Point2d::Dot(Point2d p) const {
    return x*p.x + y*p.y;
}

double Point2d::DistanceToLine(const Point2d &p0, const Point2d &dp, bool asSegment) const {
    double m = dp.x*dp.x + dp.y*dp.y;
    if(m < LENGTH_EPS*LENGTH_EPS) return VERY_POSITIVE;

    // Let our line be p = p0 + t*dp, for a scalar t from 0 to 1
    double t = (dp.x*(x - p0.x) + dp.y*(y - p0.y))/m;

    if(asSegment) {
        if(t < 0.0) return DistanceTo(p0);
        if(t > 1.0) return DistanceTo(p0.Plus(dp));
    }
    Point2d closest = p0.Plus(dp.ScaledBy(t));
    return DistanceTo(closest);
}

double Point2d::DistanceToLineSigned(const Point2d &p0, const Point2d &dp, bool asSegment) const {
    double m = dp.x*dp.x + dp.y*dp.y;
    if(m < LENGTH_EPS*LENGTH_EPS) return VERY_POSITIVE;

    Point2d n = dp.Normal().WithMagnitude(1.0);
    double dist = n.Dot(*this) - n.Dot(p0);
    if(asSegment) {
        // Let our line be p = p0 + t*dp, for a scalar t from 0 to 1
        double t = (dp.x*(x - p0.x) + dp.y*(y - p0.y))/m;
        double sign = (dist > 0.0) ? 1.0 : -1.0;
        if(t < 0.0) return DistanceTo(p0) * sign;
        if(t > 1.0) return DistanceTo(p0.Plus(dp)) * sign;
    }

    return dist;
}

Point2d Point2d::Normal() const {
    return { y, -x };
}

bool Point2d::Equals(Point2d v, double tol) const {
    double dx = v.x - x; if(dx < -tol || dx > tol) return false;
    double dy = v.y - y; if(dy < -tol || dy > tol) return false;

    return (this->Minus(v)).MagSquared() < tol*tol;
}

BBox BBox::From(const Vector &p0, const Vector &p1) {
    BBox bbox;
    bbox.minp.x = min(p0.x, p1.x);
    bbox.minp.y = min(p0.y, p1.y);
    bbox.minp.z = min(p0.z, p1.z);

    bbox.maxp.x = max(p0.x, p1.x);
    bbox.maxp.y = max(p0.y, p1.y);
    bbox.maxp.z = max(p0.z, p1.z);
    return bbox;
}

Vector BBox::GetOrigin() const { return minp.Plus(maxp.Minus(minp).ScaledBy(0.5)); }
Vector BBox::GetExtents() const { return maxp.Minus(minp).ScaledBy(0.5); }

void BBox::Include(const Vector &v, double r) {
    minp.x = min(minp.x, v.x - r);
    minp.y = min(minp.y, v.y - r);
    minp.z = min(minp.z, v.z - r);

    maxp.x = max(maxp.x, v.x + r);
    maxp.y = max(maxp.y, v.y + r);
    maxp.z = max(maxp.z, v.z + r);
}

bool BBox::Overlaps(const BBox &b1) const {
    Vector t = b1.GetOrigin().Minus(GetOrigin());
    Vector e = b1.GetExtents().Plus(GetExtents());

    return fabs(t.x) < e.x && fabs(t.y) < e.y && fabs(t.z) < e.z;
}

bool BBox::Contains(const Point2d &p, double r) const {
    return p.x >= (minp.x - r) &&
           p.y >= (minp.y - r) &&
           p.x <= (maxp.x + r) &&
           p.y <= (maxp.y + r);
}

const std::vector<double>& SolveSpace::StipplePatternDashes(StipplePattern pattern) {
    static bool initialized;
    static std::vector<double> dashes[(size_t)StipplePattern::LAST + 1];
    if(!initialized) {
        // Inkscape ignores all elements that are exactly zero instead of drawing
        // them as dots, so set those to 1e-6.
        dashes[(size_t)StipplePattern::CONTINUOUS] =
            {};
        dashes[(size_t)StipplePattern::SHORT_DASH] =
            { 1.0, 2.0 };
        dashes[(size_t)StipplePattern::DASH] =
            { 1.0, 1.0 };
        dashes[(size_t)StipplePattern::DASH_DOT] =
            { 1.0, 0.5, 1e-6, 0.5 };
        dashes[(size_t)StipplePattern::DASH_DOT_DOT] =
            { 1.0, 0.5, 1e-6, 0.5, 1e-6, 0.5 };
        dashes[(size_t)StipplePattern::DOT] =
            { 1e-6, 0.5 };
        dashes[(size_t)StipplePattern::LONG_DASH] =
            { 2.0, 0.5 };
        dashes[(size_t)StipplePattern::FREEHAND] =
            { 1.0, 2.0 };
        dashes[(size_t)StipplePattern::ZIGZAG] =
            { 1.0, 2.0 };
    }

    return dashes[(size_t)pattern];
}

double SolveSpace::StipplePatternLength(StipplePattern pattern) {
    static bool initialized;
    static double lengths[(size_t)StipplePattern::LAST + 1];
    if(!initialized) {
        for(size_t i = 0; i < (size_t)StipplePattern::LAST; i++) {
            const std::vector<double> &dashes = StipplePatternDashes((StipplePattern)i);
            double length = 0.0;
            for(double dash : dashes) {
                length += dash;
            }
            lengths[i] = length;
        }
    }

    return lengths[(size_t)pattern];
}
