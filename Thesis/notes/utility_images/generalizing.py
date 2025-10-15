import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns

sns.set_theme(style="white", context="talk")

palette = {
    "curve": "#6e6e6e",
    "highlight": "#ef4444",
    "baseline": "#c4c4c4"
}

x = np.linspace(-3, 3, 400)
risk = 0.4 + 0.05 * (x + 0.5) ** 2 + 0.02 * np.cos(3 * x)
risk_emp = risk + 0.06 * np.sin(5 * x)

fig, axes = plt.subplots(1, 3, figsize=(12, 3.4), sharey=True)
titles = ["Approximation", "Generalization", "Optimization"]

for ax, title in zip(axes, titles):
    ax.plot(x, risk, color=palette["curve"], linewidth=2.2, label=r"$R$")
    ax.plot(x, risk_emp, color=palette["curve"], linewidth=2.2, alpha=0.55, label=r"$R_n$")
    ax.set_xlim(-2.2, 2.2)
    ax.set_ylim(0.2, 0.65)
    ax.set_xticks([])
    ax.set_yticks([])
    ax.set_title(title, fontweight="bold", fontsize=14)
    ax.spines[["top", "right", "left", "bottom"]].set_visible(False)

    baseline_y = 0.22
    ax.hlines(baseline_y, -2.2, 2.2, color=palette["baseline"], linewidth=2)
    ticks_x = [-1.5, -0.3, 0.3, 1.6]
    ticks_labels = ["$f$", "$f^*$", "$\\hat f$", "$g$"]
    for tx, lbl in zip(ticks_x, ticks_labels):
        ax.text(tx, baseline_y - 0.03, lbl, ha="center", va="top", fontsize=12)
    ax.plot(ticks_x, [baseline_y] * len(ticks_x), "o", color=palette["baseline"], markersize=5)

axes[0].fill_betweenx(
    [baseline_y, risk[np.argmin(np.abs(x + 0.3))]],
    ticks_x[1],
    ticks_x[2],
    color=palette["highlight"],
    alpha=0.55
)

axes[1].fill_between(
    x,
    risk,
    risk_emp,
    where=risk_emp > risk,
    color=palette["highlight"],
    alpha=0.5
)

opt_x = ticks_x[2]
axes[2].vlines(
    opt_x,
    baseline_y,
    risk_emp[np.argmin(np.abs(x - opt_x))],
    color=palette["highlight"],
    linewidth=3,
    linestyles="--"
)
axes[2].hlines(
    risk_emp[np.argmin(np.abs(x - opt_x))],
    opt_x,
    ticks_x[3],
    color=palette["highlight"],
    linewidth=3,
    linestyles="--"
)
axes[2].fill_between(
    x,
    risk,
    risk_emp,
    where=x > opt_x,
    color=palette["highlight"],
    alpha=0.08
)

fig.tight_layout()
fig.savefig("generalizing_decomposition.png", dpi=300, bbox_inches="tight")
plt.show()