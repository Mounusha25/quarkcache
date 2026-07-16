import type { Metadata } from "next";
import ThemeRegistry from "./ThemeRegistry";
import AppShell from "@/components/layout/AppShell";

export const metadata: Metadata = {
  title: "QuarkCache",
  description: "Redis-like in-memory key-value store with real-time dashboard",
};

export default function RootLayout({ children }: { children: React.ReactNode }) {
  return (
    <html lang="en">
      <body>
        <ThemeRegistry>
          <AppShell>{children}</AppShell>
        </ThemeRegistry>
      </body>
    </html>
  );
}
