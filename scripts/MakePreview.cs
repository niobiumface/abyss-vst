using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Text;

static class Gif
{
    public static void Save(string path, List<Bitmap> frames, int delayCs)
    {
        ImageCodecInfo encoder = null;
        foreach (var candidate in ImageCodecInfo.GetImageEncoders())
        {
            if (candidate.MimeType == "image/gif")
                encoder = candidate;
        }

        var first = new EncoderParameters(1);
        first.Param[0] = new EncoderParameter(System.Drawing.Imaging.Encoder.SaveFlag, (long) EncoderValue.MultiFrame);
        frames[0].Save(path, encoder, first);

        var next = new EncoderParameters(1);
        next.Param[0] = new EncoderParameter(System.Drawing.Imaging.Encoder.SaveFlag, (long) EncoderValue.FrameDimensionTime);
        for (int i = 1; i < frames.Count; ++i)
            frames[0].SaveAdd(frames[i], next);

        var flush = new EncoderParameters(1);
        flush.Param[0] = new EncoderParameter(System.Drawing.Imaging.Encoder.SaveFlag, (long) EncoderValue.Flush);
        frames[0].SaveAdd(flush);

        var bytes = File.ReadAllBytes(path);
        for (int i = 0; i + 6 < bytes.Length; ++i)
        {
            if (bytes[i] == 0x21 && bytes[i + 1] == 0xF9 && bytes[i + 2] == 0x04)
            {
                bytes[i + 4] = (byte) (delayCs & 255);
                bytes[i + 5] = (byte) ((delayCs >> 8) & 255);
            }
        }
        int packed = bytes[10];
        int colorTable = (packed & 0x80) != 0 ? 3 * (1 << ((packed & 7) + 1)) : 0;
        int insertAt = 13 + colorTable;
        var loop = new byte[]
        {
            0x21, 0xFF, 11,
            (byte) 'N', (byte) 'E', (byte) 'T', (byte) 'S', (byte) 'C', (byte) 'A', (byte) 'P', (byte) 'E',
            (byte) '2', (byte) '.', (byte) '0',
            3, 1, 0, 0, 0
        };
        using (var stream = File.Create(path))
        {
            stream.Write(bytes, 0, insertAt);
            stream.Write(loop, 0, loop.Length);
            stream.Write(bytes, insertAt, bytes.Length - insertAt);
        }
    }
}

static class Preview
{
    const int NativeW = 240;
    const int NativeH = 186;
    const int ScreenX = 64;
    const int ScreenY = 38;
    const int Scale = 3;

    static readonly string[] Glyphs = new string[]
    {
        " ", "     |     |     |     |     |     |     ",
        "D", "#### |#   #|#   #|#   #|#   #|#   #|#### ",
        "P", "#### |#   #|#   #|#### |#    |#    |#    ",
        "%", "##  #|## # |  #  |  #  | #   |# ## |#  ##",
        "0", " ### |#   #|#  ##|# # #|##  #|#   #| ### ",
        "1", "  #  | ##  |  #  |  #  |  #  |  #  | ### ",
        "2", " ### |#   #|    #|   # |  #  | #   |#####",
        "3", "#### |    #|    #| ### |    #|    #|#### ",
        "4", "#   #|#   #|#   #|#####|    #|    #|    #",
        "5", "#####|#    |#### |    #|    #|#   #| ### ",
        "6", " ### |#    |#    |#### |#   #|#   #| ### ",
        "7", "#####|    #|   # |  #  |  #  |  #  |  #  ",
        "8", " ### |#   #|#   #| ### |#   #|#   #| ### ",
        "9", " ### |#   #|#   #| ####|    #|    #| ### ",
        "b", "#    |#    |###  |#  # |#  # |###  |     ",
        "y", "     |     |#   #| # # |  #  | #   |#    ",
        "n", "     |     |## # |#  # |#  # |#   #|     ",
        "f", "  ## | #   |#### | #   | #   | #   |     ",
        "p", "     |     |###  |#  # |###  |#    |#    ",
        "r", "     |     |# ## |##   |#    |#    |     ",
        "o", "     |     | ### |#   #|#   #|#   #| ### ",
        "-", "     |     |     |#####|     |     |     ",
        ".", "     |     |     |     |     | ##  | ##  "
    };

    static readonly Dictionary<char, string[]> Glyph = new Dictionary<char, string[]>();

    static readonly Color[] Palette =
    {
        Hex("050408"), Hex("0C0A12"), Hex("16122A"), Hex("221B37"), Hex("1B2539"),
        Hex("31294B"), Hex("2B3B55"), Hex("473D67"), Hex("5F5585"), Hex("6E6658"),
        Hex("ABA28C"), Hex("E9E2CC"), Hex("2E070B"), Hex("6A0E16"), Hex("A8161F"),
        Hex("E23A32"), Hex("0B2E34"), Hex("1A7F8A"), Hex("59E3EA")
    };

    static readonly string[] Names =
    {
        "K0", "K1", "V0", "V1", "B1", "V2", "B2", "V3", "V4",
        "N0", "N1", "N2", "R0", "R1", "R2", "R3", "C0", "C1", "C2"
    };

    static int Main(string[] args)
    {
        string root = args.Length > 0 ? args[0] : Directory.GetCurrentDirectory();
        string outDir = Path.Combine(root, "docs", "preview");
        Directory.CreateDirectory(outDir);
        for (int i = 0; i < Glyphs.Length; i += 2)
        {
            var rows = Glyphs[i + 1].Split('|');
            Glyph[Glyphs[i][0]] = rows;
        }

        using (var frame = new Bitmap(Path.Combine(root, "ui", "ui_frame.png")))
        using (var depthStrip = new Bitmap(Path.Combine(root, "ui", "knob_depth_strip.png")))
        using (var pressureStrip = new Bitmap(Path.Combine(root, "ui", "knob_pressure_strip.png")))
        using (var smallStrip = new Bitmap(Path.Combine(root, "ui", "knob_small_strip.png")))
        using (var bypass = new Bitmap(Path.Combine(root, "ui", "bypass_off.png")))
        {
            WriteDepth(root, frame, depthStrip, pressureStrip, smallStrip, bypass, Path.Combine(outDir, "depth.gif"));
            WritePressure(root, frame, depthStrip, pressureStrip, smallStrip, bypass, Path.Combine(outDir, "pressure.gif"));
        }
        Console.WriteLine("previews written to " + outDir);
        return 0;
    }

    static void WriteDepth(string root, Bitmap frame, Bitmap depthStrip, Bitmap pressureStrip, Bitmap smallStrip, Bitmap bypass, string path)
    {
        var frames = new List<Bitmap>();
        int[] levels = { 0, 2, 4, 6, 8, 10 };
        int[] sequence = { 0, 1, 0, 1, 0, 1, 0, 2 };
        foreach (int level in levels)
        {
            foreach (int anim in sequence)
            {
                using (var screen = LoadScreen(root, level * 10, anim))
                    frames.Add(Compose(frame, screen, depthStrip, pressureStrip, smallStrip, bypass, level * 10, 0));
            }
        }
        Gif.Save(path, frames, 13);
        foreach (var shot in frames)
            shot.Dispose();
    }

    static void WritePressure(string root, Bitmap frame, Bitmap depthStrip, Bitmap pressureStrip, Bitmap smallStrip, Bitmap bypass, string path)
    {
        var frames = new List<Bitmap>();
        int[] pressures = { 0, 33, 67, 100 };
        int[] sequence = { 0, 1, 0, 1, 0, 1, 0, 2 };
        foreach (int pressure in pressures)
        {
            int stage = (int) Math.Round(pressure * 3 / 100.0);
            foreach (int anim in sequence)
            {
                using (var screen = LoadScreen(root, 100, anim))
                {
                    ApplyPressure(screen, stage, anim);
                    frames.Add(Compose(frame, screen, depthStrip, pressureStrip, smallStrip, bypass, 100, pressure));
                }
            }
        }
        Gif.Save(path, frames, 13);
        foreach (var shot in frames)
            shot.Dispose();
    }

    static Bitmap LoadScreen(string root, int depth, int frame)
    {
        string name = "depth_" + depth.ToString("000") + "_f" + frame + ".png";
        return new Bitmap(Path.Combine(root, "anim", name));
    }

    static Bitmap Compose(Bitmap cabinet, Bitmap screen, Bitmap depthStrip, Bitmap pressureStrip, Bitmap smallStrip, Bitmap bypass, int depth, int pressure)
    {
        var native = new Bitmap(NativeW, NativeH, PixelFormat.Format32bppArgb);
        Fill(native, Palette[0]);
        Blit(native, screen, ScreenX, ScreenY, 0, 0, screen.Width, screen.Height);
        Blit(native, cabinet, 0, 0, 0, 0, cabinet.Width, cabinet.Height);
        Blit(native, depthStrip, 43, 132, 0, KnobFrame(depth, 0, 100, 64) * 24, 24, 24);
        Blit(native, smallStrip, 79, 140, 0, KnobFrame(0, -24, 24, 32) * 12, 12, 12);
        Blit(native, smallStrip, 101, 140, 0, KnobFrame(0, -24, 12, 32) * 12, 12, 12);
        Blit(native, pressureStrip, 133, 132, 0, KnobFrame(pressure, 0, 100, 64) * 24, 24, 24);
        Blit(native, bypass, 182, 136, 0, 0, bypass.Width, bypass.Height);
        string readout = ValueText(depth, pressure);
        DrawText(native, readout, PlateX(readout), 115, Palette[18]);
        DrawText(native, "by n-f.pro", 70, 174, Palette[10]);

        var scaled = ScaleNearest(native);
        native.Dispose();
        return scaled;
    }

    static int PlateX(string text)
    {
        int width = text.Length * 6 - 1;
        return (74 + 165 + 1 - width) / 2;
    }

    static string ValueText(int depth, int pressure)
    {
        return "D " + depth + "%  P " + pressure + "%";
    }

    static int KnobFrame(double value, double min, double max, int frames)
    {
        double norm = (value - min) / (max - min);
        int index = (int) Math.Round(norm * (frames - 1));
        if (index < 0) return 0;
        if (index >= frames) return frames - 1;
        return index;
    }

    static void Fill(Bitmap bmp, Color color)
    {
        var pixels = new int[bmp.Width * bmp.Height];
        int argb = color.ToArgb();
        for (int i = 0; i < pixels.Length; ++i)
            pixels[i] = argb;
        Write(bmp, pixels);
    }

    static void Blit(Bitmap dest, Bitmap src, int dx, int dy, int sx, int sy, int w, int h)
    {
        var dp = Read(dest);
        var sp = Read(src);
        for (int y = 0; y < h; ++y)
        {
            int yy = dy + y;
            int ys = sy + y;
            if (yy < 0 || yy >= dest.Height || ys < 0 || ys >= src.Height)
                continue;
            for (int x = 0; x < w; ++x)
            {
                int xx = dx + x;
                int xs = sx + x;
                if (xx < 0 || xx >= dest.Width || xs < 0 || xs >= src.Width)
                    continue;
                int c = sp[ys * src.Width + xs];
                int a = (c >> 24) & 255;
                if (a < 16)
                    continue;
                dp[yy * dest.Width + xx] = c | unchecked((int) 0xFF000000);
            }
        }
        Write(dest, dp);
    }

    static void DrawText(Bitmap bmp, string text, int x, int y, Color color)
    {
        int pen = x;
        foreach (char ch in text)
        {
            string[] rows;
            if (!Glyph.TryGetValue(ch, out rows))
                rows = Glyph[' '];
            for (int row = 0; row < rows.Length && row < 7; ++row)
            {
                string line = rows[row];
                for (int col = 0; col < 5 && col < line.Length; ++col)
                {
                    if (line[col] == '#' && pen + col >= 0 && pen + col < bmp.Width && y + row >= 0 && y + row < bmp.Height)
                        bmp.SetPixel(pen + col, y + row, color);
                }
            }
            pen += 6;
        }
    }

    static void ApplyPressure(Bitmap screen, int stage, int frame)
    {
        if (stage <= 0)
            return;
        var map = new int[19];
        for (int i = 0; i < 19; ++i)
            map[i] = i;
        if (stage == 1)
        {
            Map(map, "V1", "R0"); Map(map, "B1", "V1");
        }
        else if (stage == 2)
        {
            Map(map, "V0", "R0"); Map(map, "V1", "R0"); Map(map, "B1", "R0"); Map(map, "V2", "R1"); Map(map, "B2", "V2");
        }
        else
        {
            Map(map, "K1", "R0"); Map(map, "V0", "R0"); Map(map, "V1", "R1"); Map(map, "B1", "R0");
            Map(map, "V2", "R1"); Map(map, "B2", "R1"); Map(map, "V3", "R2"); Map(map, "V4", "R2");
            Map(map, "N0", "R2"); Map(map, "N1", "R3");
        }

        var src = Read(screen);
        var dst = (int[]) src.Clone();
        var shifts = Shifts(stage, frame);
        if (shifts != null)
        {
            foreach (var shift in shifts)
            {
                int y = shift.Key;
                int dx = shift.Value;
                if (y < 0 || y >= screen.Height)
                    continue;
                for (int x = 0; x < screen.Width; ++x)
                {
                    int from = x - dx;
                    if (from < 0) from = 0;
                    if (from >= screen.Width) from = screen.Width - 1;
                    dst[y * screen.Width + x] = src[y * screen.Width + from];
                }
            }
        }

        for (int i = 0; i < dst.Length; ++i)
        {
            int index = Nearest(dst[i]);
            if (index >= 0 && index < 19)
                dst[i] = Palette[map[index]].ToArgb();
        }
        Write(screen, dst);
    }

    static Dictionary<int, int> Shifts(int stage, int frame)
    {
        if (stage == 2 && frame == 0) return Dict(17, 1, 46, -1);
        if (stage == 2 && frame == 1) return Dict(30, 1, 31, 1, 58, -1);
        if (stage == 2 && frame == 2) return Dict(9, -1, 52, 1);
        if (stage >= 3 && frame == 0) return Dict(12, 2, 13, 2, 40, -1, 41, -2, 64, 1);
        if (stage >= 3 && frame == 1) return Dict(24, -2, 25, -2, 26, -1, 50, 2, 51, 1);
        if (stage >= 3 && frame == 2) return Dict(6, 1, 33, -2, 34, -2, 60, 2, 61, 2);
        return null;
    }

    static Dictionary<int, int> Dict(params int[] pairs)
    {
        var d = new Dictionary<int, int>();
        for (int i = 0; i < pairs.Length; i += 2)
            d[pairs[i]] = pairs[i + 1];
        return d;
    }

    static void Map(int[] map, string from, string to)
    {
        map[IndexOf(from)] = IndexOf(to);
    }

    static int IndexOf(string name)
    {
        for (int i = 0; i < Names.Length; ++i)
            if (Names[i] == name)
                return i;
        return 0;
    }

    static Bitmap ScaleNearest(Bitmap source)
    {
        var dest = new Bitmap(source.Width * Scale, source.Height * Scale, PixelFormat.Format32bppArgb);
        var src = Read(source);
        var dst = new int[dest.Width * dest.Height];
        for (int y = 0; y < dest.Height; ++y)
        {
            int sy = y / Scale;
            for (int x = 0; x < dest.Width; ++x)
                dst[y * dest.Width + x] = src[sy * source.Width + (x / Scale)];
        }
        Write(dest, dst);
        return dest;
    }

    static byte[] Index(Bitmap bmp)
    {
        var pixels = Read(bmp);
        var index = new byte[pixels.Length];
        for (int i = 0; i < pixels.Length; ++i)
            index[i] = (byte) Nearest(pixels[i]);
        return index;
    }

    static int Nearest(int argb)
    {
        int r = (argb >> 16) & 255;
        int g = (argb >> 8) & 255;
        int b = argb & 255;
        int best = 0;
        int bestD = int.MaxValue;
        for (int i = 0; i < Palette.Length; ++i)
        {
            int dr = r - Palette[i].R;
            int dg = g - Palette[i].G;
            int db = b - Palette[i].B;
            int d = dr * dr + dg * dg + db * db;
            if (d < bestD)
            {
                bestD = d;
                best = i;
            }
        }
        return best;
    }

    static int[] Read(Bitmap bmp)
    {
        var data = bmp.LockBits(new Rectangle(0, 0, bmp.Width, bmp.Height), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
        var pixels = new int[bmp.Width * bmp.Height];
        try
        {
            unsafeCopy(data, pixels, true);
        }
        finally
        {
            bmp.UnlockBits(data);
        }
        return pixels;
    }

    static void Write(Bitmap bmp, int[] pixels)
    {
        var data = bmp.LockBits(new Rectangle(0, 0, bmp.Width, bmp.Height), ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
        try
        {
            unsafeCopy(data, pixels, false);
        }
        finally
        {
            bmp.UnlockBits(data);
        }
    }

    static void unsafeCopy(BitmapData data, int[] pixels, bool read)
    {
        int w = data.Width;
        int h = data.Height;
        int stride = data.Stride;
        var raw = new byte[stride * h];
        if (read)
            System.Runtime.InteropServices.Marshal.Copy(data.Scan0, raw, 0, raw.Length);
        for (int y = 0; y < h; ++y)
        {
            int row = y * stride;
            int pix = y * w;
            for (int x = 0; x < w; ++x)
            {
                int o = row + x * 4;
                if (read)
                    pixels[pix + x] = raw[o] | (raw[o + 1] << 8) | (raw[o + 2] << 16) | (raw[o + 3] << 24);
                else
                {
                    int c = pixels[pix + x];
                    raw[o] = (byte) (c & 255);
                    raw[o + 1] = (byte) ((c >> 8) & 255);
                    raw[o + 2] = (byte) ((c >> 16) & 255);
                    raw[o + 3] = (byte) ((c >> 24) & 255);
                }
            }
        }
        if (!read)
            System.Runtime.InteropServices.Marshal.Copy(raw, 0, data.Scan0, raw.Length);
    }

    static byte[] PaletteBytes()
    {
        var bytes = new byte[Palette.Length * 3];
        for (int i = 0; i < Palette.Length; ++i)
        {
            bytes[i * 3] = Palette[i].R;
            bytes[i * 3 + 1] = Palette[i].G;
            bytes[i * 3 + 2] = Palette[i].B;
        }
        return bytes;
    }

    static Color Hex(string hex)
    {
        return Color.FromArgb(255,
            Convert.ToInt32(hex.Substring(0, 2), 16),
            Convert.ToInt32(hex.Substring(2, 2), 16),
            Convert.ToInt32(hex.Substring(4, 2), 16));
    }
}
