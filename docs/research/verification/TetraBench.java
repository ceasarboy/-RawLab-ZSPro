import java.util.random.RandomGenerator;

/**
 * 验证④：3D LUT 三线性 vs 四面体插值的 CPU 吞吐基准 + 正确性交叉验证。
 * 编译: javac TetraBench.java   运行: java TetraBench
 * 输出: 恒等 LUT 精确性检查、双方法最大偏差、每像素耗时。
 */
public class TetraBench {
    static final int K = 33;               // lattice 每轴节点
    static final int N = 4_000_000;        // 基准像素数
    static final float[] lut = new float[K * K * K * 3];

    static int idx(int i, int j, int k) { return ((k * K) + j) * K + i; }

    public static void main(String[] args) {
        buildLut();
        // ---- 正确性 1: 恒等 LUT 必须被两种插值精确还原（任意分辨率） ----
        float[] ident = new float[K * K * K * 3];
        for (int k = 0; k < K; k++)
            for (int j = 0; j < K; j++)
                for (int i = 0; i < K; i++) {
                    int o = idx(i, j, k) * 3;
                    ident[o] = i / (float) (K - 1);
                    ident[o + 1] = j / (float) (K - 1);
                    ident[o + 2] = k / (float) (K - 1);
                }
        RandomGenerator rng = RandomGenerator.of("L64X128MixRandom");
        float[][] pts = new float[200001][3];
        for (float[] p : pts) { p[0] = rng.nextFloat(); p[1] = rng.nextFloat(); p[2] = rng.nextFloat(); }
        double errTri = 0, errTet = 0;
        for (float[] p : pts) {
            float[] a = trilinear(ident, p[0], p[1], p[2]);
            float[] b = tetrahedral(ident, p[0], p[1], p[2]);
            errTri = Math.max(errTri, Math.max(Math.abs(a[0] - p[0]), Math.max(Math.abs(a[1] - p[1]), Math.abs(a[2] - p[2]))));
            errTet = Math.max(errTet, Math.max(Math.abs(b[0] - p[0]), Math.max(Math.abs(b[1] - p[1]), Math.abs(b[2] - p[2]))));
        }
        System.out.printf("恒等LUT还原误差   trilinear=%.3e tetrahedral=%.3e (应≈机器精度)%n", errTri, errTet);

        // ---- 正确性 2: 风格 LUT 上双方法一致性 ----
        double diff = 0;
        for (float[] p : pts) {
            float[] a = trilinear(lut, p[0], p[1], p[2]);
            float[] b = tetrahedral(lut, p[0], p[1], p[2]);
            diff = Math.max(diff, Math.max(Math.abs(a[0] - b[0]), Math.max(Math.abs(a[1] - b[1]), Math.abs(a[2] - b[2]))));
        }
        System.out.printf("风格LUT双方法最大偏差 %.4f (同一晶格上的插值误差量级)%n", diff);

        // ---- 基准 ----
        for (int round = 0; round < 2; round++) { trilinear(lut, pts[0][0], pts[0][1], pts[0][2]); tetrahedral(lut, pts[0][0], pts[0][1], pts[0][2]); }
        double bestTri = Double.MAX_VALUE, bestTet = Double.MAX_VALUE;
        long sink = 0;
        for (int round = 0; round < 7; round++) {
            long t0 = System.nanoTime();
            for (float[] p : pts) { float[] o = trilinear(lut, p[0], p[1], p[2]); sink += (long) o[0]; }
            double dt = (System.nanoTime() - t0) / (double) N;
            bestTri = Math.min(bestTri, dt);
        }
        for (int round = 0; round < 7; round++) {
            long t0 = System.nanoTime();
            for (float[] p : pts) { float[] o = tetrahedral(lut, p[0], p[1], p[2]); sink += (long) o[1]; }
            double dt = (System.nanoTime() - t0) / (double) N;
            bestTet = Math.min(bestTet, dt);
        }
        System.out.printf("trilinear   %.1f ns/px  (%.0f MP/s)%n", bestTri, 1000.0 / bestTri);
        System.out.printf("tetrahedral %.1f ns/px  (%.0f MP/s)%n", bestTet, 1000.0 / bestTet);
        System.out.printf("比值 tetra/tri = %.2fx   (sink=%d 防止消除)%n", bestTet / bestTri, sink & 1);
    }

    static void buildLut() {
        float[] m = {0.95f, 0.06f, 0f, 0.03f, 0.93f, 0.04f, 0f, 0.09f, 0.91f};
        float[] g = {1.06f, 0.94f, 1.13f}, lift = {0.015f, 0f, 0.030f};
        for (int k = 0; k < K; k++)
            for (int j = 0; j < K; j++)
                for (int i = 0; i < K; i++) {
                    float[] v = {i / (float) (K - 1), j / (float) (K - 1), k / (float) (K - 1)};
                    for (int c = 0; c < 3; c++) v[c] = (float) Math.pow(v[c], g[c]) + lift[c];
                    int o = idx(i, j, k) * 3;
                    for (int r = 0; r < 3; r++) {
                        double s = m[r * 3] * v[0] + m[r * 3 + 1] * v[1] + m[r * 3 + 2] * v[2];
                        lut[o + r] = (float) Math.min(1, Math.max(0, s));
                    }
                }
    }

    static float[] trilinear(float[] L, float x, float y, float z) {
        float px = Math.max(0, Math.min(1, x)) * (K - 1), py = Math.max(0, Math.min(1, y)) * (K - 1), pz = Math.max(0, Math.min(1, z)) * (K - 1);
        int i = Math.min((int) px, K - 2), j = Math.min((int) py, K - 2), k = Math.min((int) pz, K - 2);
        float fx = px - i, fy = py - j, fz = pz - k;
        float[] out = new float[3];
        for (int c = 0; c < 3; c++) {
            float c000 = L[(idx(i, j, k)) * 3 + c],     c100 = L[(idx(i + 1, j, k)) * 3 + c];
            float c010 = L[(idx(i, j + 1, k)) * 3 + c], c110 = L[(idx(i + 1, j + 1, k)) * 3 + c];
            float c001 = L[(idx(i, j, k + 1)) * 3 + c], c101 = L[(idx(i + 1, j, k + 1)) * 3 + c];
            float c011 = L[(idx(i, j + 1, k + 1)) * 3 + c], c111 = L[(idx(i + 1, j + 1, k + 1)) * 3 + c];
            float c00 = c000 + fx * (c100 - c000), c10 = c010 + fx * (c110 - c010);
            float c01 = c001 + fx * (c101 - c001), c11 = c011 + fx * (c111 - c011);
            float c0 = c00 + fy * (c10 - c00), c1 = c01 + fy * (c11 - c01);
            out[c] = c0 + fz * (c1 - c0);
        }
        return out;
    }

    static float[] tetrahedral(float[] L, float x, float y, float z) {
        float px = Math.max(0, Math.min(1, x)) * (K - 1), py = Math.max(0, Math.min(1, y)) * (K - 1), pz = Math.max(0, Math.min(1, z)) * (K - 1);
        int i = Math.min((int) px, K - 2), j = Math.min((int) py, K - 2), k = Math.min((int) pz, K - 2);
        float r = px - i, g = py - j, b = pz - k;
        int o1x = 0, o1y = 0, o1z = 0, o2x = 0, o2y = 0, o2z = 0, o3x = 0, o3y = 0, o3z = 0;
        float w1, w2, w3;
        if (r >= g) {
            if (g >= b) { o1x = 1; o2x = 1; o2y = 1; o3x = 1; o3y = 1; o3z = 1; w1 = r - g; w2 = g - b; w3 = b; }
            else        { o1x = 1; o2x = 1; o2z = 1; o3x = 1; o3y = 1; o3z = 1; w1 = r - b; w2 = b - g; w3 = g; }
        } else {
            if (g >= b) { o1y = 1; o2x = 1; o2y = 1; o3x = 1; o3y = 1; o3z = 1; w1 = g - r; w2 = r - b; w3 = b; }
            else        { o1y = 1; o2y = 1; o2z = 1; o3x = 1; o3y = 1; o3z = 1; w1 = g - b; w2 = b - r; w3 = r; }
        }
        float[] out = new float[3];
        int b0 = idx(i, j, k) * 3;
        int b1 = idx(i + o1x, j + o1y, k + o1z) * 3;
        int b2 = idx(i + o2x, j + o2y, k + o2z) * 3;
        int b3 = idx(i + o3x, j + o3y, k + o3z) * 3;
        for (int c = 0; c < 3; c++) {
            float c0 = L[b0 + c];
            out[c] = c0 + w1 * (L[b1 + c] - c0) + w2 * (L[b2 + c] - c0) + w3 * (L[b3 + c] - c0);
        }
        return out;
    }
}
