package jp.xenia.emulator;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout;

import com.google.android.material.card.MaterialCardView;

import java.util.List;

public class GameGridFragment extends Fragment {

    private static final String ARG_PAGE = "page";

    private GameAdapter mAdapter;
    private TextView mEmptyText;

    public static GameGridFragment newInstance(int page) {
        final GameGridFragment f = new GameGridFragment();
        final Bundle args = new Bundle();
        args.putInt(ARG_PAGE, page);
        f.setArguments(args);
        return f;
    }

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater,
                             @Nullable ViewGroup container,
                             @Nullable Bundle savedInstanceState) {
        return inflater.inflate(R.layout.fragment_game_grid, container, false);
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        final RecyclerView grid = view.findViewById(R.id.grid_games);
        mEmptyText = view.findViewById(R.id.text_empty);

        final int columns = Math.max(2, (int) (getResources().getDisplayMetrics().widthPixels
                / getResources().getDisplayMetrics().density / 120));
        grid.setLayoutManager(new GridLayoutManager(requireContext(), columns));

        mAdapter = new GameAdapter();
        grid.setAdapter(mAdapter);

        final SwipeRefreshLayout swipe = view.findViewById(R.id.swipe_refresh);
        swipe.setColorSchemeResources(R.color.green_500);
        swipe.setOnRefreshListener(() -> {
            mAdapter.notifyDataSetChanged();
            swipe.setRefreshing(false);
        });

        updateEmpty();
    }

    public void refresh() {
        if (mAdapter != null) mAdapter.notifyDataSetChanged();
        updateEmpty();
    }

    private void updateEmpty() {
        if (mEmptyText == null) return;
        final boolean empty = LauncherActivity.sGames.isEmpty();
        mEmptyText.setVisibility(empty ? View.VISIBLE : View.GONE);
    }

    private class GameAdapter extends RecyclerView.Adapter<GameAdapter.ViewHolder> {

        @NonNull
        @Override
        public ViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
            final View v = LayoutInflater.from(parent.getContext())
                    .inflate(R.layout.card_game, parent, false);
            return new ViewHolder(v);
        }

        @Override
        public void onBindViewHolder(@NonNull ViewHolder holder, int position) {
            final LauncherActivity.GameEntry game = LauncherActivity.sGames.get(position);
            holder.title.setText(game.title);
            holder.region.setText(game.region);
            // Tap → launch; long press → Dolphin-style properties dialog
            holder.itemView.setOnClickListener(v ->
                    requireActivity().startActivity(
                            EmulatorActivity.createInternalIntent(
                                    requireContext(), game.uri, game.title)));
            holder.itemView.setOnLongClickListener(v -> {
                GamePropertiesDialog.newInstance(position)
                        .show(requireActivity().getSupportFragmentManager(), GamePropertiesDialog.TAG);
                return true;
            });

            // Load box art: embedded XEX → TheGamesDB → placeholder
            BoxArtManager.load(requireContext(), game, holder.art);
        }

        @Override
        public int getItemCount() { return LauncherActivity.sGames.size(); }

        class ViewHolder extends RecyclerView.ViewHolder {
            final TextView title;
            final TextView region;
            final android.widget.ImageView art;
            ViewHolder(View v) {
                super(v);
                title = v.findViewById(R.id.text_game_title);
                region = v.findViewById(R.id.text_game_region);
                art = v.findViewById(R.id.image_game_art);
            }
        }
    }
}
